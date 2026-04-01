# kmod_loader -- Adaptive Module Loader

`kmod_loader` is a userspace tool that patches a freestanding `.ko` binary at load time to match the running kernel's ABI. It enables cross-kernel module loading without recompilation.

## Usage

```
kmod_loader <module.ko> [kallsyms_addr=0xHEX] [options] [param=value ...]
```

## What It Patches

| Field | How |
|-------|-----|
| **vermagic** | Replaced with running kernel's `uname -r` + standard suffix |
| **CRC values** | `__versions` section patched with correct CRCs for `module_layout`, `printk`, etc. |
| **init/exit offsets** | Relocations in `.rela.gnu.linkonce.this_module` adjusted to match `struct module` layout |
| **struct module size** | `.gnu.linkonce.this_module` section resized |
| **printk symbol** | `_printk` (6.1+) vs `printk` (5.10) auto-detected and renamed in `__versions` + strtab |
| **kallsyms_addr** | Patched directly into ELF `.data` section (avoids `module_param` / shadow CFI issues) |
| **this_module section** | Zeroed before patching to prevent `ei_funcs`/`num_ei_funcs` garbage |

## CLI Options

| Option | Description |
|--------|-------------|
| `kallsyms_addr=0xHEX` | Address of `kallsyms_lookup_name` (required for most modules) |
| `--init-off 0xHEX` | Override `struct module` init function offset |
| `--exit-off 0xHEX` | Override `struct module` exit function offset |
| `--probe` | Force re-probe of init/exit offsets (ignore cache) |
| `--crc sym=0xHEX` | Override CRC for a specific symbol (repeatable) |
| `param=value` | Module parameters passed through to `init_module` |

## Init/Exit Offset Resolution

`kmod_loader` uses a tiered strategy to find the correct `init` and `exit` offsets in `struct module`:

1. **Persistent cache** -- previously probed offsets stored in the `kmod_loader` binary itself
2. **Version presets** -- hardcoded offsets for known kernel versions (4.4 through 6.12)
3. **Method A (kcore disasm)** -- reads `do_init_module` from `/proc/kcore`, scans for `LDR X0, [Xn, #imm]; CBZ X0; BL do_one_initcall` pattern
4. **Method B (binary probe)** -- embeds a minimal `probe_module.ko` (init returns `-EINVAL`), tries candidate offsets 0x140-0x200 step 8. `EINVAL` = init ran = correct offset. Crash-resilient via companion state file.

Use `--init-off` / `--exit-off` to skip auto-detection.

## CRC Resolution

CRC values are resolved in order:

1. `--crc` command-line overrides
2. kallsyms `__crc_*` symbols (from `/proc/kallsyms`)
3. Vendor `.ko` files on device (parse `__versions` section)
4. Boot partition kernel image
5. `finit_module` with `MODULE_INIT_IGNORE_MODVERSIONS` flag (if kernel supports it)

## AVD-Specific Usage

Android Virtual Devices lack `/proc/kcore` and vendor `.ko` files. CRCs must be extracted from the host-side kernel image:

```bash
# Extract CRCs from AVD kernel image
CRC_ARGS=$(python3 scripts/extract_avd_crcs.py -s emulator-5554)

# Load with extracted CRCs
adb shell "/data/local/tmp/kmod_loader /data/local/tmp/my_hook.ko \
    kallsyms_addr=0x... $CRC_ARGS"
```

## Build

```bash
cd tools/kmod_loader
make              # build kmod_loader (host binary)
make regenerate   # rebuild embedded probe_module.ko (needs cross-compiler)
```

The `probe_module_embed.h` is pre-committed, so `make` only needs a host C compiler.

## Example

```bash
# Physical device (Pixel 6, kernel 6.1)
ADDR=$(adb shell "su -c 'cat /proc/kallsyms'" | awk '/kallsyms_lookup_name$/{print "0x"$1}')
adb push kmod_loader hello_hook.ko /data/local/tmp/
adb shell "su -c '/data/local/tmp/kmod_loader /data/local/tmp/hello_hook.ko kallsyms_addr=$ADDR'"

# AVD (kernel 5.10)
CRC_ARGS=$(python3 scripts/extract_avd_crcs.py -s emulator-5554)
adb shell "/data/local/tmp/kmod_loader /data/local/tmp/hello_hook.ko \
    kallsyms_addr=$ADDR $CRC_ARGS"
```
