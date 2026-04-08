# AVD Kernel Module Testing

Automated test infrastructure for running KernelHook kernel module tests across Android Virtual Device emulators.

## Quick Start

```bash
# Test all supported AVDs
./scripts/test_avd_kmod.sh

# Test specific AVDs
./scripts/test_avd_kmod.sh Pixel_31 Pixel_34 Pixel_37

# Userspace test sweep on a running emulator (auto adb-root + setenforce 0)
./scripts/run_android_tests.sh --serial emulator-5554 --build-dir build_android

# Manual single-device kmod test (USB / magisk only)
./scripts/run_android_tests.sh --kmod
```

> `run_android_tests.sh` auto-issues `adb root` + `setenforce 0` on userdebug
> emulators so userspace tests can `mprotect RW→RX`. The kmod path (`--kmod`)
> still uses `su -c` helpers and is intended for USB/magisk devices; for
> emulator kmod regression use `test_avd_kmod.sh` above.

## Prerequisites

- Android SDK with emulator and system images (API 28+)
- Android NDK r29+ (auto-detected from `~/Library/Android/sdk/ndk/`)
- Python 3 (for CRC extraction)
- AVDs created for target API levels (e.g., `Pixel_31`, `Pixel_37`)

## Test Flow

For each AVD, `test_avd_kmod.sh` performs:

1. **Kill existing emulators** and start the target AVD
2. **Wait for boot** and establish `adb root`
3. **Build** `kh_test.ko` with kernel-version-specific flags
4. **Extract CRCs** from host-side `kernel-ranchu` image
5. **Push** `kh_test.ko` and `kmod_loader` to device
6. **Load** module via `kmod_loader` with extracted CRCs
7. **Capture** dmesg for test results (25 tests per kernel)
8. **Unload** module and kill emulator

## Build Configuration

The `tests/kmod/Makefile` auto-configures per kernel version:

| Feature | Condition | Effect |
|---------|-----------|--------|
| `-mcmodel=large` | kernel <= 4.x | Avoids ADRP relocations unsupported by 4.x module loader |
| `-fsanitize=kcfi` | kernel >= 6.1 | Enables kCFI metadata for 6.1+ kernels |
| `THIS_MODULE_SIZE=0x800` | all versions | Padded so `kmod_loader` can always shrink to actual device size |
| `MODULE_INIT_OFFSET` | per version | GKI preset; `kmod_loader` overrides via vendor introspection |

## CRC Extraction

`scripts/extract_avd_crcs.py` extracts symbol CRCs from the host-side `kernel-ranchu` image. It auto-detects three ksymtab entry formats:

| Format | Size | Kernels | CRC size |
|--------|------|---------|----------|
| prel32 | 12B  | 5.10+   | 4B       |
| absolute | 16B | 4.x   | 4B or 8B |
| absolute+CFI | 24B | 5.4 | 4B     |

For unrelocated images (4.14 and earlier), the tool falls back to `__ksymtab_<sym>` address lookup from `/proc/kallsyms`.

```bash
# Standalone usage
python3 scripts/extract_avd_crcs.py -s emulator-5554 module_layout printk memcpy memset
# Output: --crc module_layout=0x7c24b32d --crc printk=0xc5850110 ...
```

## Known Limitations

| Kernel | Issue | Root Cause |
|--------|-------|------------|
| 3.18 (API 25-27) | Skipped | Module loader hangs on large freestanding modules (MOVW relocation overhead) |
| 5.4 (API 30) | Fixed | Was: `_error_injection_whitelist` section overflow + wrong `exit_off` preset (0x350→0x340) |
| Pixel_36 | Boot timeout | AVD system image does not boot; API 36 is covered by Pixel_36.1 |

## Creating AVDs for Testing

```bash
# Install system images
sdkmanager "system-images;android-31;google_apis;arm64-v8a"
sdkmanager "system-images;android-37.0;google_apis_ps16k;arm64-v8a"

# Create AVDs
avdmanager create avd -n Pixel_31 -k "system-images;android-31;google_apis;arm64-v8a" -d pixel
avdmanager create avd -n Pixel_37 -k "system-images;android-37.0;google_apis_ps16k;arm64-v8a" -d pixel
```
