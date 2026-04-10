# Ring 3 fix on GKI 6.6+ — bump `THIS_MODULE_SIZE` default in `kmod/shim/shim.h`

**Status:** Open. Created 2026-04-10. Highest-priority follow-up of
the sh_size fix branch (PR #1, merged in `d96953d`).

## Background

PR #1 fixed `kh_test.ko` load failure on Pixel_35/Pixel_37 (GKI 6.6 /
6.12) by shrinking the ELF `.gnu.linkonce.this_module` section size at
load time inside `tools/kmod_loader/kmod_loader.c:patch_module_layout()`.
Verification matrix from PR #1:

| AVD | Kernel | Ring 1 | Ring 3 |
|---|---|---|---|
| Pixel_33 | 5.15 | ✅ | ✅ |
| Pixel_34 | 6.1 | ✅ | ✅ |
| Pixel_35 | 6.6 | ✅ | ❌ — exporter.ko load rejected |
| Pixel_37 | 6.12 | ✅ | ❌ — exporter.ko load rejected |

Ring 3 6.6+ failure is **not a regression** — on `main @ 4037ea6`
those AVDs failed Ring 1 first, so Ring 3 was never reached. PR #1
unblocking Ring 1 exposed this latent issue.

## Root cause

Verified empirically during PR #1's session:

- `exporter.ko` and `importer.ko` are built via `kmod/mk/kmod.mk`,
  which uses `kmod/shim/shim.h:305`'s default
  `#define THIS_MODULE_SIZE 0x440`.
- The resulting `.ko` files have `.gnu.linkonce.this_module` `sh_size`
  = `0x440` (1088 bytes). Confirmed via
  `llvm-readelf -S exporter.ko`:
  ```
  [11] .gnu.linkonce.this_module PROGBITS … 0044c0 000440 …
  ```
- Android 15 GKI 6.6 expects `sizeof(struct module) = 0x600` (1536
  bytes); GKI 6.12 expects `0x640` (1600 bytes).
- `0x440 < 0x600` → `kmod_loader`'s shrink helper guard
  (`*sh_size_ptr <= preset->mod_size`) returns 0 (no-op, since the
  helper is shrink-only). ELF stays at `0x440` → kernel rejects with
  the same printk PR #1's investigation identified:
  ```
  module exporter: .gnu.linkonce.this_module section size must
                   match the kernel's built struct module size at
                   run time
  ```

## Fix

**One-line bump in `kmod/shim/shim.h`:**

```diff
 #ifndef THIS_MODULE_SIZE
-#define THIS_MODULE_SIZE 0x440
+#define THIS_MODULE_SIZE 0x800
 #endif
```

This raises the default padding for **all shim-based builds**
(`exporter.ko`, `importer.ko`, any future `kmod.mk`-built module) to
`0x800` so the existing `kmod_loader` shrink helper (already merged
via PR #1) can shrink down to whatever the running kernel requires.

`tests/kmod/Makefile` already overrides `THIS_MODULE_SIZE=0x800`
explicitly for `kh_test.ko`, so this change has no effect on that
build.

## Why a 1-line fix is enough

- 5.4/5.10/5.15/6.1: kernels don't enforce the size check. Helper
  shrinks `0x800 → 0x400` (preset value). Same code path that already
  passes Ring 1 + Ring 3 in PR #1's verification.
- 6.6: kernel enforces. Helper shrinks `0x800 → 0x600`.
- 6.12: kernel enforces. Helper shrinks `0x800 → 0x640`.

PR #1's `maybe_shrink_this_module_sh_size()` already handles all four
cases uniformly; the only thing missing is making sure shim-built
modules start at a size large enough to be shrunk.

## Risks

| Risk | Mitigation |
|---|---|
| Bumping the default enlarges every shim-built `.ko` by `0x800 - 0x440 = 0x3c0` (960 bytes) | Negligible: `exporter.ko` 51992 → 52952 bytes (~1.8% growth). No constraint anywhere. |
| Some consumer hard-codes the literal `0x440` | Search before changing — should only appear in `kmod/shim/shim.h:305`. Quick check: `rg '0x440' kmod/ tools/ tests/` |
| Plan 5 (`docs/followups/2026-04-10-plan5-kh-test-kbuild.md`) touches the same header | Plan 5 is about `tests/kmod/*.c` consuming `shim.h` differently in kbuild mode; the `THIS_MODULE_SIZE` default is unrelated. Land this before or after Plan 5 — order does not matter. |

## Verification

After applying the bump:

```bash
./scripts/test_avd_kmod.sh Pixel_33 Pixel_34 Pixel_35 Pixel_37
```

**Required result:**
- Ring 1 4/4 PASS (already verified by PR #1, must remain green)
- Ring 3 **4/4 PASS** (this is the new gate)
- Pixel_35/Pixel_37 loader trace shows `kmod_loader: shrink this_module sh_size 0x800 -> 0x600` (or `0x640`) for both `kh_test.ko` AND `exporter.ko`/`importer.ko`

Also verify (no regressions on what PR #1 already shipped):
```bash
cd tools/kmod_loader
make test_patch_this_module && ./test_patch_this_module   # 8/8 PASS unchanged
make test_resolver          && ./test_resolver            # PASS unchanged
./tests/test_config_parser.sh                              # PASS unchanged
```

And Contract 4:
```bash
cd tools/kh_crc
rm kh_crc && make
./tests/run_tests.sh   # 3/3 PASS, no byte changes
```

## Estimated effort

15 minutes including test matrix run.

## Reference

- PR #1 — sh_size fix: bmax121/KernelHook#1 (merged in `d96953d`)
- PR #1 design spec: `docs/superpowers/specs/2026-04-09-gki-6.6-sh_size-fix-design.md` (verification log §)
- PR #1 investigation: `docs/superpowers/investigations/2026-04-09-gki-6.6-6.12-load-failure.md`
- PR #1 handoff (full context): `docs/superpowers/handoffs/2026-04-10-gki-6.6-sh_size-fix-handoff.md`
