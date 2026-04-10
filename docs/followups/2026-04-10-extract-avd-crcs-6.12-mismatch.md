# `extract_avd_crcs.py` 6.12 `module_layout` CRC mismatch

**Status:** Open. Created 2026-04-10. Independent second-order bug
discovered during PR #1's verification on Pixel_37 (GKI 6.12).
Non-blocking — module still loads (tainted) because
`CONFIG_MODULE_SIG_FORCE` is unset on this AVD.

## Symptom

After PR #1's sh_size fix lets `kh_test.ko` load on Pixel_37 (GKI
6.12.58-android16-6), the running kernel prints:

```
kh_test: disagrees about version of symbol module_layout
```

The module loads anyway (tainted), and all 25 hook tests pass. But
the kernel disagrees with the CRC value `kmod_loader` patched into
the `__versions` table.

## What we know

- The CRC was supplied via `--crc module_layout=0x797f2b3e` on the
  loader CLI, sourced from `scripts/extract_avd_crcs.py` reading the
  `kernel-ranchu` image at
  `~/Library/Android/sdk/system-images/android-37.0/google_apis_ps16k/arm64-v8a/kernel-ranchu`.
- The running kernel rejects that value as wrong, indicating the
  kernel computes a different CRC at runtime than what's stored at
  `__crc_module_layout` in the on-disk image we're parsing.
- This issue is **6.12-specific**. On Pixel_33 (5.15), Pixel_34 (6.1),
  and Pixel_35 (6.6), the same script produces CRCs that the running
  kernel accepts without warning.
- Pixel_37's kernel has `CONFIG_CFI_CLANG=y`, `CONFIG_RANDSTRUCT_NONE=y`,
  `CONFIG_MODVERSIONS=y`, and `CONFIG_MODULE_SIG_PROTECT=y` (Android
  GKI variant) but `# CONFIG_MODULE_SIG_FORCE is not set`.

## Hypotheses to investigate

1. **kCFI changed where the CRC is stored on 6.12.** The
   `__crc_<sym>` symbol layout or relocation type may have changed in
   the linker scripts of GKI 6.12 vs 6.6, and `extract_avd_crcs.py`'s
   parser is reading from the pre-6.12 location.
2. **Different `__crc_` linkage on the kernel image vs runtime
   resolution.** The disk image's `__crc_module_layout` symbol may
   live in a section that gets stripped or relocated by the kernel
   loader in a way the script doesn't model.
3. **`module_layout` symbol no longer participates in the CRC scheme
   on 6.12 the way it used to.** Other GKI versions still expose it
   via `__ksymtab_module_layout` (verified via `/proc/kallsyms`), but
   the CRC anchor for it may have moved.

## Suggested probe

1. **Compare runtime CRC vs script CRC**: write a tiny probe `.ko`
   that, after load, dumps `find_symbol("module_layout")->crc` (or
   the equivalent in 6.12's `mod_kallsyms` API) via printk. Read it
   from dmesg. Compare to `extract_avd_crcs.py`'s output for the same
   symbol on the same AVD.
2. **Inspect `__ksymtab_module_layout` structure on disk vs in
   memory**: dump the section from `kernel-ranchu` and from
   `/sys/kernel/btf/vmlinux` (if exposed) on Pixel_37. Look for layout
   differences vs Pixel_35.
3. **Diff the script's parser logic against the 6.12 linker script**:
   read `arch/arm64/kernel/vmlinux.lds.S` from
   `android-common-kernel`'s `android16-6.12` branch, find where
   `__crc_*` symbols land, compare to what `extract_avd_crcs.py`
   assumes.

## Why it's not blocking PR #1's fix

- `kh_test.ko` loads successfully and runs all 25 tests on Pixel_37.
- The taint warning is purely cosmetic in our test environment
  (`MODULE_SIG_FORCE` is off).
- This bug existed before PR #1 — PR #1 just exposed it by being the
  first thing to actually load a module on Pixel_37.

## When it WILL matter

- A device with `CONFIG_MODULE_SIG_FORCE=y` will reject the module
  outright instead of tainting. Most Android user-build devices fall
  into this category for production modules — only adb-root userdebug
  builds tolerate the taint.
- Any consumer of `kmod_loader` that wants clean dmesg output (no
  taint warnings) will hit this on 6.12+.

## Out of scope for this followup

Fixing this requires kernel-source-aware investigation that's deeper
than a quick patch. Treat as its own investigation+fix cycle, not a
simple bug.

## Reference

- PR #1 investigation §1.6 (where this was first noted): `docs/superpowers/investigations/2026-04-09-gki-6.6-6.12-load-failure.md`
- `scripts/extract_avd_crcs.py` — the script that needs the fix
- Pixel_37 kernel: `6.12.58-android16-6-gccafb60de224-ab14828483`
