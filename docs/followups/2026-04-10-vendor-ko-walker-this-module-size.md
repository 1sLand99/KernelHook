# Preset-free `VAL_THIS_MODULE_SIZE` fallback via vendor `.ko` walker

**Status:** Open. Created 2026-04-10. Deferred from PR #1 by user
decision (focus PR #1 on the bug fix; ship optimization separately).

## Background

PR #1 (`d96953d`) added `maybe_shrink_this_module_sh_size()` to
`tools/kmod_loader/kmod_loader.c:patch_module_layout()`. The shrink
target value is `preset->mod_size`, resolved by the existing
resolver chain for `VAL_THIS_MODULE_SIZE`:

```
CLI → PO (probe_ondisk_module) → CE (config_explicit)
    → CA (config_automatch from kmod/devices/*.conf)
    → CF (config_fuzzy) → PD (probe_disasm) → END
```

Today, only **CA** actually returns a value for `VAL_THIS_MODULE_SIZE`
on a real device — by reading the `this_module_size` field from a
`kmod/devices/*.conf` preset that matches the running kernel's
release prefix (e.g., `kernelrelease = 6.6.`).

**Problem:** Every new GKI release line requires manually measuring
`sizeof(struct module)` for that kernel and adding a new
`kmod/devices/*.conf` entry, otherwise `kmod_loader` falls back to
`mod_size = 0` and the shrink helper is skipped — re-exposing the
ENOEXEC bug PR #1 fixed.

## Goal

Make `kmod_loader` self-sufficient on any GKI device (today and
future), with zero `kmod/devices/*.conf` maintenance burden, by
**probing the running kernel's `sizeof(struct module)` from any
already-installed vendor `.ko` on disk**.

This is sound because vendor `.ko` files are built from the **same
GKI kernel tree** as the running kernel — their
`.gnu.linkonce.this_module` ELF section size **is** the kernel's
runtime `sizeof(struct module)`. Authoritative, byte-exact, no
heuristics.

## Approach (Tasks 3 + 4 from PR #1's plan)

The implementation plan from PR #1 already specifies the exact code
changes step-by-step in
`docs/superpowers/plans/2026-04-09-gki-6.6-sh_size-fix.md` §§ "Task 3"
and "Task 4". Summary:

### Task 3 — extend `crc_from_vendor_ko()` cache

`tools/kmod_loader/kmod_loader.c` already has `crc_from_vendor_ko()`
(around line 326) which:

1. Walks `/vendor_dlkm/lib/modules`, `/vendor/lib/modules`,
   `/system/lib/modules`, `/odm/lib/modules`, `/lib/modules`
2. Opens the first readable `.ko`
3. Parses its ELF to extract the `__versions` CRC table
4. Caches the CRC table in static state for subsequent calls

**Extension:** while the ELF is already parsed in step 3, also extract
the `.gnu.linkonce.this_module` section's `sh_size` and cache it in a
new file-scope static `g_ko_this_module_size`. Add an accessor:

```c
int sizeof_struct_module_from_vendor_ko(uint32_t *out);
```

It triggers the cache population (via a dummy `crc_from_vendor_ko`
call) on first use, then returns `g_ko_this_module_size` if non-zero.

Estimated +30 LoC in `tools/kmod_loader/kmod_loader.c`.

### Task 4 — wire into resolver

`tools/kmod_loader/strategies/probe_ondisk.c` currently has a no-op
fallthrough for non-CRC values. Add a `VAL_THIS_MODULE_SIZE` branch
that calls `sizeof_struct_module_from_vendor_ko()` and populates the
`resolved_t` with `source_label = "probe_ondisk:vendor_ko_this_module"`.

Because PO sits **before** CA in the resolver chain, this becomes the
authoritative source whenever a vendor `.ko` exists, with the conf
preset as a fallback for hosts that don't.

Estimated +15 LoC in `tools/kmod_loader/strategies/probe_ondisk.c`.

### Combined diff

~50 LoC across two files. No new files. No new tests strictly
required (file-IO-heavy walker is best verified at AVD integration
level), but a unit test in `tests/test_resolver.c` confirming the new
PO branch is reachable would be cheap.

## Verification

```bash
./scripts/test_avd_kmod.sh Pixel_33 Pixel_34 Pixel_35 Pixel_37
```

**Expected new behavior** (visible in loader trace):

- Pixel_35: `kmod_loader: sizeof(struct module): 0x600 (from /vendor/lib/modules/virtio_input.ko)` followed by the existing shrink line.
- Pixel_37: same but `0x640` (from `failover.ko` or similar).
- All four AVDs still PASS Ring 1.

**Bonus test — confirm preset-free path actually fires**: temporarily
remove or invalidate the `kmod/devices/gki_6.6_android15.conf` file's
`this_module_size = 0x600` line, re-run on Pixel_35, confirm the
loader still produces the correct shrink trace and the module loads.
This proves PO took over from CA. Then restore the conf file.

## Risks

| Risk | Mitigation |
|---|---|
| Some vendor `.ko` is stripped or has unusual ELF and parse fails | Walker is best-effort across all candidates; CA preset path remains the secondary source |
| `/vendor/lib/modules` is empty on some weird AVD | The PO walker already has 5 directories to try; if all fail, CA still resolves on known kernels |
| Cache invalidation across `kmod_loader` invocations | Each invocation is a fresh process; the static cache lives only for the duration of one load. Correct semantics. |

## Estimated effort

1-2 hours including:
- ~45 min to write the code (mechanical from the plan)
- ~30 min to run AVD verification matrix
- ~30 min to write a brief follow-up commit message + small unit test

## Reference

- PR #1 — sh_size fix: bmax121/KernelHook#1 (merged in `d96953d`)
- Implementation plan: `docs/superpowers/plans/2026-04-09-gki-6.6-sh_size-fix.md` §§ Task 3, Task 4
- Existing resolver chain definition: `tools/kmod_loader/resolver.c:53-58`
- Existing vendor walker: `tools/kmod_loader/kmod_loader.c:~326` (`crc_from_vendor_ko`)
