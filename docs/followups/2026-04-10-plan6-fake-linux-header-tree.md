# Plan 6 — Fake Linux header tree (architectural cleanup)

**Status:** Open. Created 2026-04-10. Mentioned but not specced in
`docs/followups/2026-04-10-plan5-kh-test-kbuild.md` § "Out of scope".
Strictly cleanup; no functional changes.

## Background

Plans 3 and 4 (`feat/kbuild-interop`) introduced a **dual-path** approach
for every header that needs to compile in both freestanding (no kernel
tree) and kbuild (with kernel headers) modes:

```c
#ifdef KMOD_FREESTANDING
#  include "shim.h"
#  include <stdarg.h>
#else
#  include <linux/kernel.h>
#  include <linux/printk.h>
#  include <linux/stdarg.h>
#endif
```

This pattern is sprinkled across `kmod/src/`, `include/arch/arm64/`,
`include/ktypes.h`, and (after Plan 5) will be in `tests/kmod/` too.
It works but it's repetitive: every consumer file has to know about
both paths and maintain two `#include` blocks in lockstep.

The user proposed during Plan 4 review: instead of dual-path each .c
file, **provide a fake `<linux/...>` header tree** that the freestanding
build path picks up via `-I`, redirecting `<linux/types.h>`,
`<linux/stdarg.h>`, `<linux/kernel.h>`, etc. to shim definitions. Then
every .c file just writes `#include <linux/types.h>` unconditionally
and the freestanding/kbuild distinction collapses to a single
`-I kmod/shim/include` flag.

## Why it's a separate plan

- **Plan 4 took the pragmatic per-header dual-path approach** to ship
  Plan 3 quickly. That decision is correct for shipping; Plan 6 is the
  architectural follow-up that pays down the technical debt.
- **Scope is large**: every header in `include/`, every shim symbol
  currently in `kmod/shim/shim.h`, and every consumer file under
  `kmod/src/`, `tests/kmod/`, `examples/`. Done wrong, this is a
  high-surface-area refactor.
- **Risk surface**: name collisions with the real `<linux/*.h>` when
  the same TU is later compiled in kbuild mode. The fake tree must
  ONLY be on the include path under `KMOD_FREESTANDING`, never both.

## Goals

1. Remove the per-file `#ifdef KMOD_FREESTANDING / #else / #endif`
   include guards from `kmod/src/*.c`, `tests/kmod/*.c`, and
   `include/arch/arm64/*.h`.
2. Replace with a single freestanding-only include path
   `kmod/shim/include/` that mirrors enough of the kernel's `linux/`,
   `asm/`, `asm-generic/` namespaces to satisfy our consumers.
3. Reduce `kmod/shim/shim.h` to a tiny umbrella that re-exports the
   fake tree's contents (or delete `shim.h` entirely if all consumers
   migrate to `<linux/...>` style).
4. Ensure `make freestanding` and `make -C $(KDIR) M=…` produce
   bit-identical `.ko` files (modulo build paths in DWARF) compared
   to today.

## Non-goals

- No new functional behavior. Pure header reorganization.
- No ABI changes to `kernelhook.ko`'s exported symbols.
- No changes to `kh_crc` (Contract 4: CRC bytes preserved).

## Suggested approach

### Phase A — proof of concept on one symbol

Pick a single symbol like `__noinline` (currently dual-path'd in
`include/arch/arm64/transit.h` and elsewhere). Create
`kmod/shim/include/linux/compiler.h` that defines it for the
freestanding path, gated behind a header guard the real
`<linux/compiler.h>` doesn't use. Update one consumer file to drop
its `#ifdef KMOD_FREESTANDING` block. Confirm both build paths still
produce a valid .ko.

### Phase B — migrate all current dual-path headers

Inventory the dual-path blocks in the tree (rg `KMOD_FREESTANDING`
inside `#ifdef`/`#else`). For each, decide whether the freestanding
side wants a header file under `kmod/shim/include/linux/...` or
`asm/...` or `asm-generic/...`. Reuse Linux kernel naming.

### Phase C — collapse `shim.h`

Once consumers go through `<linux/...>` directly, `kmod/shim/shim.h`
should shrink to either:
- A single umbrella `#include <linux/kernel.h>` (and the umbrella
  pulls in everything), or
- Be deleted entirely with consumers picking individual headers.

### Phase D — verify CI matrix

The CI matrix from Plan 5 (once Plan 5 lands) should remain green:

- `android14-6.1`: `kernelhook.ko`, `kbuild_hello.ko`, `kh_test.ko`
  all build, all assertions pass
- `tools/kh_crc/tests/run_tests.sh`: 3/3 PASS, no byte changes
- `scripts/test_avd_kmod.sh Pixel_30 Pixel_33 Pixel_34 Pixel_35 Pixel_37`:
  Ring 1 5/5 PASS, Ring 3 5/5 PASS (after Plan 5 + the Ring 3 padding
  bump in `docs/followups/2026-04-10-ring3-shim-padding-bump.md`)

## Naming conflicts to watch

- `<linux/types.h>` already exists in real kernel headers. If a TU is
  ever compiled in kbuild mode with `-I kmod/shim/include` on the
  search path, the fake will shadow the real one — bad. Plan 6 must
  enforce that `kmod/shim/include` is added to `-I` **only when**
  `KMOD_FREESTANDING` is set.
- `<linux/compiler_attributes.h>`'s `__noinline__` macro collision
  was the bug Plan 4 commit `45a30f5`/`b080b8f` worked around. The
  fake header must not redefine it; just provide a stub.
- Consumer .c files may currently rely on shim.h pulling in *implicit*
  declarations. After Plan 6 they'll need explicit `<linux/foo.h>`
  includes, which is fine but means each migration is a small audit.

## Out of scope for Plan 6

- Migrating the existing **kbuild-side** include order. Plan 6 only
  touches the freestanding side; kbuild already gets real Linux
  headers and is correct as-is.
- Rewriting `tools/` host code (e.g., `kmod_loader`). Those use libc
  not kernel headers; unaffected.

## Verification gates

- Both `make freestanding` and `make -C $KDIR M=$(pwd) modules`
  succeed for `tests/kmod/kh_test.ko` after migration.
- A spot-check of one migrated `.c` file shows the dual-path
  `#ifdef KMOD_FREESTANDING` block is gone, replaced with a single
  `#include <linux/kernel.h>` (or similar).
- No regressions in `scripts/test_avd_kmod.sh` or
  `tools/kh_crc/tests/run_tests.sh`.
- No new `__has_include` guards needed in user-facing headers.

## Reference

- Plan 5 — `docs/followups/2026-04-10-plan5-kh-test-kbuild.md`
- Plan 4 commits that established the dual-path pattern: `e4aa5c1`,
  `45a30f5`, `b080b8f`, `3ea947b`, `f6a592f`, `84108bd`
- Plan 3 (Mode C kernelhook.ko) commits: `72eb511`, `8462d22`,
  `c230e86`, `9e9c3d9`, `e41fa12`
