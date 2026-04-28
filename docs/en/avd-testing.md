# AVD Kernel Module Testing

Automated test infrastructure for running KernelHook across Android emulators.

Two paths exist, picked by AVD API level:

- **kmod** (`test_avd_kmod.sh`) — Pixel_28..34. Loads self-built `kernelhook.ko` directly via `kmod_loader`.
- **graft** (`test_avd_graft.sh`) — Pixel_35..37. Splices the KernelHook payload into a vendor `.ko` to bypass Android 15+ GKI's kCFI initcall typeid check.

`./scripts/test.sh avd-sdk-all` auto-routes by AVD trailing API number (≥35 → graft).

## Quick Start

```bash
# Full matrix (Pixel_28..34 kmod + Pixel_35..37 graft)
./scripts/test.sh avd-sdk-all

# Subset
./scripts/test_avd_kmod.sh  Pixel_31 Pixel_34
./scripts/test_avd_graft.sh Pixel_35

# Reuse a running AVD instead of killing+launching
./scripts/test_avd_kmod.sh  --keep-emulator Pixel_31
./scripts/test_avd_graft.sh --keep-emulator Pixel_35

# Userspace ctest on a running emulator
./scripts/run_android_tests.sh --serial emulator-5554 --build-dir build_android
```

`--keep-emulator` scans running emulators for one whose `emu avd name` matches and reuses that serial — lets multiple AVDs (or unrelated ones like a kptest VM) coexist instead of forcing a kill-all on entry.

## Prerequisites

- Android SDK with emulator + system images (API 28+)
- Android NDK r29+ (auto-detected from `$ANDROID_NDK_ROOT` / `~/Library/Android/sdk/ndk/`)
- Python 3 + `pyyaml` (for CRC extraction + golden TSV regen)

## SDK Mode Flow

`test_avd_kmod.sh --mode=sdk` (default) per AVD:

1. Boot/reuse the AVD; `adb root`; `setenforce 0`; `kptr_restrict=0`
2. Build `kernelhook-{prel32,abs64,abs64-legacy[-u32]}.ko` (4 layout variants) once across the matrix
3. Extract baseline CRCs (`module_layout`, `_printk`/`printk`, `memcpy`, `memset`); `kmod_loader` auto-resolves the rest from vendor modules on the device
4. Push artifacts; load `kernelhook.ko` with `kh_consistency_check=1`
5. Iterate consumers — `ksyms_lookup` first (no hook spam), then the four hook-installing ones — verifying each init marker captured **inline** in the same adb shell as the loader (avoids dmesg-eviction race once multiple `[permanent]` hooks fire)
6. On the `hello_hook` PASS, snapshot `[kh_strategy]` lines from dmesg → write `tests/golden/strategy_matrix/values/<avd>.yaml.new` (drift-only; mv to accept)

`test_avd_graft.sh` uses a single `hello_hook` consumer by default (`--consumers=` to expand). It auto-builds `kh_payload.o` if `make module-dual` cleaned it.

## Loader Variant Selection

`kmod_loader` probes the live kernel and picks the right `kernelhook-*.ko`:

| ksymtab stride | Variant            | Kernels                            |
|----------------|--------------------|-------------------------------------|
| 12 B (PREL32)  | `prel32`           | 5.10+ GKI                           |
| 24 B           | `abs64`            | 5.3..5.7 (namespace field present)  |
| 16 B (8 B CRC) | `abs64-legacy`     | 4.4 (Android 9)                     |
| 16 B (4 B CRC) | `abs64-legacy-u32` | 4.14 backport (Android 10)          |

CMODEL: `-mcmodel=large` is on for all variants (MOVW only — pre-GKI 4.x module loaders reject `adrp`).

## Known Limitations

| Kernel / AVD     | State    | Note                                                                                       |
|------------------|----------|--------------------------------------------------------------------------------------------|
| 3.18 (Pixel_27)  | Skipped  | Pre-`abs64-legacy`; module loader hangs on large freestanding modules                      |
| API 36 baseline  | Use 36.1 | `Pixel_36` system image fails to boot on macOS arm64; `Pixel_36_1` covers the API level    |
| Real Pixel 6 / 6.1.99-android14-11 | Open | Self-built kmod load triggers hardlockup-watchdog → reboot. Try graft path before patching kernelhook (see `memory/project_pixel6_61_99_panic.md`). |

## Creating AVDs

```bash
sdkmanager "system-images;android-31;google_apis;arm64-v8a"
avdmanager create avd -n Pixel_31 -k "system-images;android-31;google_apis;arm64-v8a" -d pixel
```
