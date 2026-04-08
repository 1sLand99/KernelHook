# Getting Started

## Prerequisites

- **Mode A (Freestanding):** Android NDK or `aarch64-linux-gnu-` cross-compiler. No kernel headers needed.
- **Mode B (SDK):** Same as Mode A. Requires `kernelhook.ko` loaded on target first.
- **Mode C (Kbuild):** Kernel source tree or headers for the target kernel.
- **Target device:** ARM64 Linux kernel (Android phone, AVD emulator, or any aarch64 Linux).

## Quick Start (Mode A)

Clone and build the `hello_hook` example:

```bash
git clone https://github.com/bmax121/KernelHook.git
cd KernelHook/examples/hello_hook
make module
```

This produces `hello_hook.ko` -- a freestanding kernel module that hooks `do_sys_openat2` and logs every `open()` syscall.

### Loading on Device

If CRC/vermagic matches the running kernel, use `insmod` directly. `insmod` does
not auto-fetch the symbol address, so pass `kallsyms_addr=` explicitly:

```bash
adb push hello_hook.ko /data/local/tmp/
ADDR=$(adb shell "su -c 'grep \" kallsyms_lookup_name$\" /proc/kallsyms'" | awk '{print "0x"$1}')
adb shell "su -c 'insmod /data/local/tmp/hello_hook.ko kallsyms_addr=$ADDR'"
```

For cross-kernel loading (CRC/vermagic mismatch), use `kmod_loader`. The loader
auto-fetches `kallsyms_lookup_name` from `/proc/kallsyms`, so no manual address
is needed:

```bash
cd ../../tools/kmod_loader
make
adb push kmod_loader hello_hook.ko /data/local/tmp/
adb shell "su -c '/data/local/tmp/kmod_loader /data/local/tmp/hello_hook.ko'"
# Override with kallsyms_addr=0xHEX if kptr_restrict hides symbols from root.
```

### Verify

```bash
adb shell dmesg | grep hello_hook
# hello_hook: hooked do_sys_open* at ffffffc0xxxxxxxx
# hello_hook: open called, filename ptr=...
```

### Unload

```bash
adb shell "su -c 'rmmod hello_hook'"
```

## Next Steps

- [Build Modes](build-modes.md) -- detailed comparison of Mode A / B / C
- [API Reference](api-reference.md) -- full hook API documentation
- [Examples](examples.md) -- all example modules with code walkthrough
- [kmod_loader](kmod-loader.md) -- adaptive module loader reference
