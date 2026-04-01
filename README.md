# KernelHook

ARM64 function hooking framework for Linux kernels.

## Features

- **Inline hook** -- replace any kernel function, call original via backup pointer
- **Hook chain** -- multiple before/after callbacks on one function, priority-ordered
- **Function pointer hook** -- hook ops table callbacks with chain support
- **Symbol resolution** -- `ksyms_lookup` / `ksyms_lookup_cache` for runtime symbol lookup
- **Three build modes** -- Freestanding (no kernel headers), SDK (shared kernelhook.ko), Kbuild (standard)
- **Adaptive loader** -- `kmod_loader` patches .ko binaries for cross-kernel loading

## Quick Start

```bash
# Build the hello_hook example (Mode A, freestanding)
cd examples/hello_hook
make module

# Build the adaptive loader
cd ../../tools/kmod_loader
make

# Push to device
adb push kmod_loader hello_hook.ko /data/local/tmp/

# Get kallsyms_lookup_name address
ADDR=$(adb shell "su -c 'cat /proc/kallsyms'" | awk '/kallsyms_lookup_name$/{print "0x"$1}')

# Load
adb shell "su -c '/data/local/tmp/kmod_loader /data/local/tmp/hello_hook.ko kallsyms_addr=$ADDR'"

# Verify
adb shell dmesg | grep hello_hook
```

## Architecture

| Component | Description |
|-----------|-------------|
| `src/arch/arm64/inline.c` | Instruction relocation engine + code patching |
| `src/arch/arm64/transit.c` | Transit stub + callback dispatch |
| `src/arch/arm64/pgtable.c` | Page table walking + PTE modification |
| `src/core_user.c` | Hook chain API (hook/unhook/hook_wrap) |
| `src/hmem.c` | Bitmap allocator for ROX/RW memory pools |
| `kmod/` | SDK, linker scripts, shim headers |
| `tools/kmod_loader/` | Adaptive module loader |
| `examples/` | hello_hook, fp_hook, hook_chain, hook_wrap_args, ksyms_lookup |

## Documentation

- [Getting Started](docs/en/getting-started.md)
- [Build Modes](docs/en/build-modes.md)
- [API Reference](docs/en/api-reference.md)
- [kmod_loader](docs/en/kmod-loader.md)
- [Examples](docs/en/examples.md)

[中文文档](README_zh.md)

## Build & Test

### Userspace Tests (macOS / Android)

```bash
# macOS (Apple Silicon)
cmake -B build_debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build_debug
cd build_debug && ctest

# Android (cross-compile)
cmake -B build_android -DCMAKE_TOOLCHAIN_FILE=cmake/android-arm64.cmake -DCMAKE_BUILD_TYPE=Debug
cmake --build build_android
adb push build_android/tests/test_* /data/local/tmp/
adb shell /data/local/tmp/test_hook_basic
```

### Kernel Module

```bash
# Mode A (freestanding, no kernel headers)
cd examples/hello_hook && make module

# Mode B (SDK, depends on kernelhook.ko)
cd examples/hello_hook && make -f Makefile.sdk module

# Mode C (Kbuild, requires kernel source)
cd examples/hello_hook && make -C /path/to/kernel M=$(pwd) modules
```

## License

GPL-2.0-or-later
