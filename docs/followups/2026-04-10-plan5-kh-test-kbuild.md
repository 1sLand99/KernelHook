# Plan 5 — Cleanup follow-up: make `tests/kmod/kh_test.ko` build under kbuild

**Status:** Open. Created 2026-04-10 as the explicit deferred work after
Plan 3 + Plan 4 (`feat/kbuild-interop`, merged into `main`).

> **⚠ Architectural directive — read first:**
> [`2026-04-10-ddk-and-local-build-scripts.md`](2026-04-10-ddk-and-local-build-scripts.md)
> mandates that the kbuild path migrate to **AOSP DDK** (Bazel-based)
> and that build invocations live in `scripts/build/` so contributors
> can run them locally. The mechanical `tests/kmod/*.c` cleanups
> below still apply unchanged — they fix kernel-header hygiene
> issues independent of the build system. The change is the **final
> step**: instead of writing a `tests/kmod/Kbuild` Makefile fragment,
> Plan 5 should write a `tests/kmod/BUILD.bazel` with `ddk_module()`.
>
> **Suggested sequence**: do Phase 1 of the DDK directive first
> (script extraction, no behavior change), then land Plan 5's
> `tests/kmod/*.c` cleanups, then DDK Phase 2/3 wraps `tests/kmod/`
> in `BUILD.bazel`.

## Background

Plan 3 delivered Mode C (`kmod/Kbuild` → `kernelhook.ko`,
`examples/kbuild_hello/` → SDK consumer, CI assertions on GKI 6.1).
Plan 4 (folded into the same branch) was the dual-path header refactor
that made all the freestanding-only headers (`include/arch/arm64/pgtable.h`,
`include/ktypes.h`, etc.) kbuild-safe.

Both deliverables landed green for `kernelhook.ko` and `kbuild_hello.ko`
in CI run 24206391890 on `android14-6.1`. **However the existing
`tests/kmod/kh_test.ko` target still fails to build under the same
workflow.** It is gated by `!cancelled()` so the Plan 3 Mode C
assertions run independently, but the job overall is still red because
of `kh_test.ko`.

## Why it's deferred

`tests/kmod/kh_test.ko` is a freestanding-shaped test harness that:

- Includes our shim/freestanding headers in non-gated ways.
- Defines its own `__noinline` / `__noinline__` declarations that hit
  the same kernel `<linux/compiler_attributes.h>` macro-collision bugs
  Plan 4 fixed for `src/arch/arm64/{transit,inline}.c`.
- Uses `KCFI_EXEMPT` without including `<hook.h>` in the kbuild path.
- Uses `<stdarg.h>` instead of `<linux/stdarg.h>` (the same fix already
  applied to `kmod/src/log.c` needs replicating).
- Pulls in our `include/arch/arm64/pgtable.h` and consumes the
  freestanding-mode runtime variables (`page_size`, function pointers)
  in ways that require the same dual-path treatment Plan 4 did for
  `inline.c` / `pgtable.c`.

None of this is on the Plan 3 critical path. The `kh_test.ko` failure
mode does not affect `kernelhook.ko` or any consumer `.ko` produced
through `kmod/Kbuild` / `examples/kbuild_hello/` / `kmod/mk/kmod_sdk.mk`.

## Concrete error inventory (last seen on run 24206391890)

```
tests/kmod/test_hook_kernel.h:8     __noinline__ undeclared identifier
tests/kmod/test_hook_kernel.h:9     __noinline__ undeclared identifier
tests/kmod/test_hook_kernel.c:64    __noinline__ undeclared identifier
tests/kmod/test_hook_kernel.c:77    __noinline__ undeclared identifier
tests/kmod/log.c:26                 unknown type name 'KCFI_EXEMPT'
tests/kmod/log.c:51                 use of undeclared identifier 'printk'
tests/kmod/test_main.c:293          assigning to 'int' from incompatible type 'void'
                                    (cascade from kh_pgtable_init rename — test_main.c
                                     was updated by Plan 4 sed but probably needs
                                     the printk → _printk treatment too)
tests/kmod/../../include/arch/arm64/pgtable.h
                                    (no longer applicable — Plan 4 already
                                     gated this header. The remaining errors
                                     are inside tests/kmod/*.c that consumed
                                     the pre-refactor names.)
```

## Suggested approach for Plan 5

Mechanical replication of the Plan 4 fixes inside `tests/kmod/`:

1. **`tests/kmod/log.c`** — apply the same edit pattern used on
   `kmod/src/log.c` in commit `e4aa5c1` / `45a30f5`:
   ```c
   #ifdef KMOD_FREESTANDING
   #include "shim.h"
   #include <ksyms.h>
   #include <stdarg.h>
   #else
   #include <linux/kernel.h>
   #include <linux/printk.h>
   #include <linux/stdarg.h>
   #endif
   #include <hook.h>   /* KCFI_EXEMPT */
   #include <log.h>
   ```
   And replace `(log_func_t)printk` → `(log_func_t)_printk`.

2. **`tests/kmod/test_hook_kernel.{h,c}`** — replace any
   `static __noinline …` or `__attribute__((… noinline …))` with the
   reserved-name form `__attribute__((__noinline__))` or
   `__attribute__((__used__, __noinline__))`. Same edits as Plan 4
   commit `b080b8f` did for `src/arch/arm64/{transit,inline}.c`.

3. **`tests/kmod/test_main.c`** — verify `kh_pgtable_init()` rename
   landed (Plan 4 sed should have caught it; double-check). If
   `pr_info("pgtable_init failed (%d)", rc)` still calls something
   that returns `void`, the cascade error needs an explicit branch.

4. **`tests/kmod/Kbuild`** — confirm it uses relative paths
   (`../../src/...`) not `$(KH_ROOT)` absolute paths. Plan 4 commit
   `f6a592f` already documented why.

5. **CI** — once `kh_test.ko` builds, drop the `!cancelled()` guard
   on the Plan 3 Mode C steps in `.github/workflows/kbuild.yml` (they
   were added in commit `d5c2d15` as a workaround for this very issue).
   The job conclusion will then be true green on android14-6.1.

## Out of scope for Plan 5

- Older kernels (5.10/5.15/6.6/6.12) currently fail at `modules_prepare`
  or `Build kh_test.ko` for unrelated reasons. Plan 5 should keep the
  CI matrix scoped to GKI 6.1 only for Mode C assertions; broadening
  belongs to a separate Plan.
- The grand "fake linux source tree" architecture (move
  `kmod/shim/shim.h` content into `kmod/shim/include/linux/*.h` so all
  `.c` files unconditionally `#include <linux/types.h>` etc.) is a
  cleanup that the user proposed during Plan 4. It's the right
  long-term direction but it's strictly a cleanup — Plan 4 took the
  pragmatic per-header dual-path approach to ship Plan 3. Track this
  as a separate "Plan 6 — fake linux header tree" follow-up.

## Verification gates

After Plan 5, the `feat/kbuild-interop`-style CI should produce:

- `android14-6.1` job: **all steps green**, including
  `Build kh_test.ko`, `Verify kh_test.ko`, `Build kernelhook.ko (Mode C)`,
  Module.symvers assertion, kbuild_hello.ko, depends assertion.
- `tools/kh_crc/tests/run_tests.sh`: 3/3 PASS unchanged.
- `tests/kmod/export_link_test`: clean rebuild unchanged.
- `scripts/test_avd_kmod.sh Pixel_30 Pixel_33 Pixel_34`: 6/6 PASS
  unchanged (Plan 2 freestanding regression).

## Reference commits (already on main as of this doc)

- `72eb511` shim.h defensive gate (Plan 3 T1)
- `8462d22` kmod/Kbuild standalone (Plan 3 T2)
- `c230e86` examples/kbuild_hello/ (Plan 3 T3)
- `9e9c3d9` CI Mode C assertions wired (Plan 3 T4)
- `e41fa12` docs/{en,zh}/build-modes.md Mode C rewrite (Plan 3 T5)
- `9fdde70` GKI branch name fix (`common-` prefix bug)
- `4ea8862` dwarves for BTF
- `e4aa5c1` ktypes.h / hook.h / log.c first wave
- `d5c2d15` !cancelled() decoupling
- `84108bd` arch/arm64 pgtable + inline dual-path (Plan 4 main)
- `3ea947b` pgtable_init → kh_pgtable_init + _printk
- `f6a592f` kmod/Kbuild relative paths
- `45a30f5` __noinline / __maybe_unused via linux/compiler.h
- `b080b8f` reserved attribute name form
