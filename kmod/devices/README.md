# KernelHook Device Profiles

Each `.conf` file in this directory describes one kernel/device combination's
ABI-specific values. `kmod_loader` uses them at runtime to patch
`kernelhook.ko` and user modules so a single `.ko` binary can load across
many supported targets.

## Format (INI-style, deterministic)

```ini
[identity]
name              = gki_6.1_android14      # stable, filename-derived, snake_case
description       = Android 14 GKI 6.1 reference  # free text
arch              = aarch64                 # currently always aarch64

[match]
kernelrelease     = 6.1.                    # prefix match on `uname -r`

[modversions]
module_layout     = 0xea759d7f              # all values hex with 0x prefix
_printk           = 0x92997ed8
memcpy            = 0x4829a47e
memset            = 0xdcb764ad

[struct_module]
this_module_size   = 0x400                  # output of `sizeof(struct module)` for this kernel
module_init_offset = 0x140                  # offsetof(struct module, init)
module_exit_offset = 0x3d8                  # offsetof(struct module, exit)

[vermagic]
string = "6.1.25-android14-11-00098-g7314d5e47c48 SMP preempt mod_unload modversions aarch64"

[provenance]                                # OPTIONAL, helps reviewers verify
verified          = true                    # or `false` for best-effort ports
source_module     = /vendor/lib/modules/kheaders.ko
extracted_by      = kmod_loader probe --verbose
extracted_at      = 2026-04-09
contributed_by    = bmax <bmax@example.com>
```

## Contribution workflow

1. On the target device, run `kmod_loader probe --verbose` to capture all
   discoverable values.
2. Copy the output into a new `.conf` file named after the device/kernel.
3. Open a PR. CI will lint the file against `schema.json`, run the unit
   tests, and verify the file parses. If an AVD image exists for the
   target, CI will also run an end-to-end load test.
4. A committer reviews and merges. That's all.

## Why this matters

Prior to Deliverable B the device-specific values were baked into
`tools/kmod_loader/kmod_loader.c` as a hardcoded `presets[]` array.
Adding a new device required a C-code edit, a rebuild, and deep knowledge
of the loader's internals. Now it's one `.conf` file, and the loader's
strategy chain auto-matches by `uname -r`.
