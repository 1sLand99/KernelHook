# Migrate kbuild path to AOSP DDK + extract build scripts to source tree

**Status:** Open. Created 2026-04-10. Architectural directive that
**modifies Plan 5's execution path** and adds a build-tooling
extraction layer underneath all kbuild-based work.

## The directive

Two coupled changes that should land together:

1. **Use AOSP DDK** (Driver Development Kit, Bazel-based) for
   building all out-of-tree GKI modules — `kernelhook.ko`,
   `kbuild_hello.ko`, and (after Plan 5) `kh_test.ko`. Stop using
   the legacy `make -C $KERNEL_OUT M=$(pwd) modules` path.
2. **Extract the build invocations from `.github/workflows/kbuild.yml`
   into shell scripts under `scripts/build/`** that contributors can
   run locally with one command. The CI workflow then becomes a
   thin wrapper that just calls those scripts in a matrix.

The two parts are coupled because DDK is what makes the local-runnable
goal feasible — a contributor today cannot easily reproduce the
existing workflow locally because it depends on cloning the full
`android-common` kernel tree (gigabytes), running `gki_defconfig`,
building `vmlinux` for `Module.symvers`, and managing `$KERNEL_OUT`
state. DDK packages all of that as a downloadable hermetic kernel
artifact, so a contributor can just `bazel run //...` against a
pinned kernel SHA without ever cloning common-kernel.

## Why now

The current `.github/workflows/kbuild.yml` (197 lines, see
`d96953d..HEAD`) embeds the entire build pipeline as inline shell:

- `apt install` host toolchain (clang, lld, llvm, gcc-aarch64-linux-gnu, dwarves, …)
- `git clone --branch android14-6.1 https://android.googlesource.com/kernel/common`
- `make gki_defconfig` → `make modules_prepare` → `make vmlinux`
- `make -C "$KERNEL_OUT" M=tests/kmod modules`
- Same for `M=kmod` (kernelhook.ko) and `M=examples/kbuild_hello`

A new contributor cannot reproduce this without copy-pasting from the
YAML and tweaking paths. Onboarding a new module to the kbuild path
requires editing the YAML, pushing, and waiting for CI to surface
errors — a slow loop. Local-runnable scripts collapse the loop to
seconds.

DDK is also the path AOSP itself uses for production GKI modules
(see `tools/bazel run //common-modules/virtual-device:virtual_device_aarch64_dist`
and similar in AOSP) and what new GKI versions assume. Sticking with
the M=KDIR path is increasingly an outlier.

## Goals

1. **DDK migration** — every `.ko` we build for GKI uses
   `ddk_module()` Bazel rules against a pinned DDK kernel package.
2. **Local-runnable** — a fresh clone of KernelHook + a single
   command produces all .ko artifacts on a contributor's machine
   (Linux x86_64 host with Bazelisk; macOS host using Linux VM or
   Docker if the DDK toolchain doesn't run natively on darwin).
3. **CI is a thin wrapper** — `.github/workflows/kbuild.yml` shrinks
   to ~30 lines: matrix definition + checkout + invoke
   `scripts/build/ci_kbuild.sh <branch>`.
4. **Backwards compatibility for the freestanding path** — the
   existing `tests/kmod/Makefile freestanding` target and
   `scripts/test_avd_kmod.sh` continue to work unchanged. DDK is
   added alongside, not instead of, the freestanding build.

## Non-goals

- Removing the freestanding build. It's still the right tool for
  AVD smoke testing and physical-device Ring 1 use cases.
- Migrating `tools/kmod_loader` or `tools/kh_crc` to Bazel — those
  are host-side tools, not modules.
- Changing `kmod/exports.manifest` or any exported symbol surface.
- Touching the AVD-side runtime path (`scripts/test_avd_kmod.sh`,
  `kmod_loader`).

## Structure

### New scripts directory layout

```
scripts/
├── build/
│   ├── README.md                  Usage and troubleshooting
│   ├── setup_ddk.sh               One-time DDK toolchain bootstrap
│   ├── fetch_kernel_package.sh    Download pinned DDK kernel artifact for a branch
│   ├── build_module_ddk.sh        Invoke Bazel for one module target
│   ├── build_all_modules.sh       Wrapper that builds the project's full module set
│   └── ci_kbuild.sh               Top-level entry the workflow calls
├── lib/
│   └── detect_toolchain.sh        (existing, unchanged)
└── test_avd_kmod.sh               (existing, unchanged)
```

### New BUILD.bazel files

```
kmod/BUILD.bazel                                ddk_module(name = "kernelhook", ...)
examples/kbuild_hello/BUILD.bazel               ddk_module(name = "kbuild_hello", deps = [":kernelhook"], ...)
tests/kmod/BUILD.bazel                          ddk_module(name = "kh_test", ...)   (Plan 5)
```

### Pinned kernel package versions

```
scripts/build/kernel_packages.bzl
```

A small registry mapping `branch → DDK kernel package URL/SHA`. Get
the current pins from the existing `.github/workflows/kbuild.yml`
matrix (`android12-5.10`, `android13-5.15`, `android14-6.1`,
`android15-6.6`, `android16-6.12`) and resolve each to a DDK release
artifact. AOSP publishes these at
`https://android.googlesource.com/kernel/common/...` with `Bazel`
build artifacts.

## Migration path (suggested phasing)

### Phase 1 — local script extraction (no DDK yet)

The smaller, lower-risk first step. Pure refactor of existing
workflow content into shell scripts; no behavior change.

1. Create `scripts/build/setup_kbuild_env.sh` containing the existing
   `apt install` + git clone + `make gki_defconfig` +
   `make modules_prepare` + `make vmlinux` flow, parameterized by
   `BRANCH` env var.
2. Create `scripts/build/build_kh_test.sh`, `scripts/build/build_kernelhook.sh`,
   `scripts/build/build_kbuild_hello.sh` — each calls `make -C $KERNEL_OUT M=...`
   for one module.
3. Create `scripts/build/ci_kbuild.sh` that orchestrates them.
4. Replace the inline steps in `.github/workflows/kbuild.yml` with
   calls to those scripts. Workflow shrinks to checkout + matrix +
   `bash scripts/build/ci_kbuild.sh "$BRANCH"`.
5. Verify CI green parity with `main` (must produce identical
   `Module.symvers` and pass all existing assertions).

After Phase 1, contributors can locally run any single piece by
calling the script directly with the right env vars — no DDK needed.

### Phase 2 — DDK introduction

1. Add Bazelisk + DDK toolchain bootstrap to `scripts/build/setup_ddk.sh`.
2. Create `scripts/build/kernel_packages.bzl` with pins for the 5
   current matrix branches.
3. Write `kmod/BUILD.bazel` with a `ddk_module()` for `kernelhook`.
   Verify `bazel run //kmod:kernelhook` produces a `.ko` whose
   `Module.symvers` exports the same 17 symbols `kmod/exports.manifest`
   declares.
4. Write `examples/kbuild_hello/BUILD.bazel` with `deps = ["//kmod:kernelhook"]`.
   Verify `kbuild_hello.ko`'s `modinfo` declares
   `depends: kernelhook` (current Mode C assertion).
5. (After Plan 5 lands) Write `tests/kmod/BUILD.bazel` for `kh_test.ko`.

### Phase 3 — switch CI to DDK

1. Update `scripts/build/ci_kbuild.sh` to call the new DDK
   `scripts/build/build_module_ddk.sh` instead of the Phase-1
   make-based scripts.
2. Drop the legacy `make -C $KERNEL_OUT M=...` paths from the
   non-DDK scripts (they remain in git history for reference but
   aren't called).
3. Verify the matrix is still green and that artifact bytes match
   (or differ only in expected reproducibility metadata) compared
   to the make-based outputs.

### Phase 4 — local-runnable docs

1. Add `docs/build-modes.md` (or extend the existing one from
   `e41fa12`) with a "Running the kbuild path locally" section
   that walks through:
   ```bash
   ./scripts/build/setup_ddk.sh
   ./scripts/build/build_all_modules.sh android14-6.1
   ls kmod/kernelhook.ko examples/kbuild_hello/kbuild_hello.ko tests/kmod/kh_test.ko
   ```
2. Document the DDK kernel package cache location and how to update
   pins.

## Impact on existing followups

### Plan 5 (`2026-04-10-plan5-kh-test-kbuild.md`)

**Scope changes.** Plan 5 currently assumes the existing M=KDIR
workflow path. Under this directive, Plan 5's "make `kh_test.ko`
build under kbuild" task becomes "make `kh_test.ko` build under
DDK". The mechanical fixes to `tests/kmod/*.c` that Plan 5 enumerates
(the `__noinline__` collisions, `KCFI_EXEMPT` includes, `<stdarg.h>`
→ `<linux/stdarg.h>`, etc.) **still apply** — those are kernel-header
hygiene issues independent of the build system. The change is the
final step: instead of writing a `tests/kmod/Kbuild`-style Makefile
fragment, write a `tests/kmod/BUILD.bazel` with `ddk_module()`.

**Sequence: do Phase 1 of this DDK directive first**, then Plan 5 can
land its `tests/kmod/*.c` cleanups against the new script
infrastructure, then Phase 2/3/4 of DDK can include
`tests/kmod/BUILD.bazel`.

### Plan 6 (`2026-04-10-plan6-fake-linux-header-tree.md`)

Unaffected. The fake `<linux/...>` header tree is for the
freestanding build path; DDK is the kbuild path. Plan 6 still applies
in the freestanding lane.

### Ring 3 padding bump (`2026-04-10-ring3-shim-padding-bump.md`)

Independent. The `kmod/shim/shim.h:305` constant affects the
freestanding (`kmod/mk/kmod.mk`) build of `exporter.ko`/`importer.ko`,
not the DDK path. Land separately, any time.

### Vendor `.ko` walker (`2026-04-10-vendor-ko-walker-this-module-size.md`)

Independent. Tools-side change to `kmod_loader`, no overlap with the
build system.

### kh_crc binary (`2026-04-10-kh-crc-binary-untracked.md`)

Independent. Repo hygiene for a host tool.

### 6.12 CRC mismatch (`2026-04-10-extract-avd-crcs-6.12-mismatch.md`)

Independent. Investigation of a script-side bug.

## Risks

| Risk | Mitigation |
|---|---|
| DDK on darwin host: AOSP's Bazel toolchain may not run natively on macOS arm64 | Phase 1 (script extraction without DDK) is darwin-friendly. Phase 2+ may require contributors on macOS to use Docker or a Linux VM. Document this in `scripts/build/README.md`. The freestanding path remains darwin-native, so AVD work is unaffected. |
| DDK kernel package size | DDK packages are ~500MB-1GB but cache once per branch. Compared to cloning full common-kernel + building vmlinux (current path), DDK is actually faster on second run. Document the cache location and a `make clean-ddk-cache` equivalent. |
| CI build time regression | DDK first-run is slower than cached make-build; second-run is faster. Net should be neutral or positive once cache is warm. Add CI cache key per branch. |
| Phase 1 script extraction introduces a bug not caught by CI | Phase 1 produces identical outputs to today's workflow by design. Verify with `diff` of `Module.symvers` and `objdump -d` of each `.ko` against a baseline build before merging. |
| Plan 5's `tests/kmod/*.c` cleanups become entangled with DDK migration | Phase 1 first (scripts only, no DDK), THEN Plan 5 (cleanups), THEN Phase 2/3 (DDK). Strictly sequenced to keep PRs small. |

## Verification gates

### Phase 1 (script extraction)

- `scripts/build/ci_kbuild.sh android14-6.1` runs locally on a fresh
  Linux host and produces:
  - `tests/kmod/kh_test.ko` (currently fails per Plan 5; Phase 1 just
    matches the current CI behavior, including the failure)
  - `kmod/kernelhook.ko` with all 17 manifest exports in `Module.symvers`
  - `examples/kbuild_hello/kbuild_hello.ko` with `depends: kernelhook`
- `.github/workflows/kbuild.yml` is ≤ 50 lines
- All current CI assertions still pass (or fail in the same places)

### Phase 2 (DDK introduction)

- `bazel run //kmod:kernelhook` from a fresh checkout produces a
  `.ko` byte-identical (modulo timestamps and reproducibility
  metadata) to the Phase 1 make-built version
- DDK builds on Linux x86_64 host machines (not just CI)
- A new contributor following `docs/build-modes.md` can produce
  `kernelhook.ko` from a clean clone in under 10 minutes (excluding
  initial DDK download)

### Phase 3 (CI cutover)

- Matrix still green
- Artifact diff against Phase 1 baseline is empty or expected-only
  (timestamps, build paths)
- Workflow is < 30 lines

### Phase 4 (docs)

- `docs/build-modes.md` has a working "Running locally" section
- A new contributor reproducing the steps gets working `.ko` files
  with no IRC/Slack help

## Estimated effort

- Phase 1 (script extraction): 1 day
- Phase 2 (DDK introduction): 2-3 days, mostly debugging Bazel
  toolchain issues on first contact
- Phase 3 (CI cutover): 0.5 day
- Phase 4 (docs): 0.5 day

Plan 5's `tests/kmod/*.c` cleanups (existing scope, unchanged) sit
between Phase 1 and Phase 2 and add another 1-2 days.

Total to fully closed end state: ~1 week of focused work, parallelizable
into 2-3 PRs.

## Reference

- AOSP DDK docs: https://source.android.com/docs/setup/build/bazel/ddk
- Existing workflow: `.github/workflows/kbuild.yml`
- Existing build mode docs: `docs/{en,zh}/build-modes.md` (Mode C
  added by Plan 3 commit `e41fa12`)
- Plan 5 (mechanical .c fixes): `docs/followups/2026-04-10-plan5-kh-test-kbuild.md`
- Plan 6 (freestanding header tree): `docs/followups/2026-04-10-plan6-fake-linux-header-tree.md`
