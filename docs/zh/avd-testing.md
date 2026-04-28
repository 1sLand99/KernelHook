# AVD 内核模块测试

跨 Android 模拟器自动化运行 KernelHook 的测试基础设施。

按 AVD API 数值分两条路：

- **kmod** (`test_avd_kmod.sh`) —— Pixel_28..34。直接通过 `kmod_loader` 加载自建 `kernelhook.ko`
- **graft** (`test_avd_graft.sh`) —— Pixel_35..37。把 KernelHook payload 嫁接到 vendor `.ko` 上，绕过 Android 15+ GKI 的 kCFI initcall typeid 检查

`./scripts/test.sh avd-sdk-all` 按 AVD 名末尾 API 数字自动分流（≥35 走 graft）。

## 快速开始

```bash
# 全量（Pixel_28..34 kmod + Pixel_35..37 graft）
./scripts/test.sh avd-sdk-all

# 子集
./scripts/test_avd_kmod.sh  Pixel_31 Pixel_34
./scripts/test_avd_graft.sh Pixel_35

# 复用已跑着的 AVD（不杀+不重启）
./scripts/test_avd_kmod.sh  --keep-emulator Pixel_31
./scripts/test_avd_graft.sh --keep-emulator Pixel_35

# 在运行中的模拟器上跑用户态 ctest
./scripts/run_android_tests.sh --serial emulator-5554 --build-dir build_android
```

`--keep-emulator` 扫描所有运行中 emulator 找匹配 `emu avd name` 的并复用——让多个 AVD（或无关的 kptest VM 等）能并存，不需要每次都 kill-all。

## 前置

- Android SDK + emulator + 系统镜像（API 28+）
- Android NDK r29+（自动从 `$ANDROID_NDK_ROOT` / `~/Library/Android/sdk/ndk/` 探测）
- Python 3 + `pyyaml`（用于 CRC 提取和 golden TSV 重生）

## SDK 模式流程

`test_avd_kmod.sh --mode=sdk`（默认）逐 AVD：

1. 启动/复用 AVD；`adb root`；`setenforce 0`；`kptr_restrict=0`
2. 矩阵入口跑一次 build：`kernelhook-{prel32,abs64,abs64-legacy[-u32]}.ko` 共 4 种 layout
3. 提取基线 CRC（`module_layout`、`_printk`/`printk`、`memcpy`、`memset`）；其余 CRC 由 `kmod_loader` 在设备上从 vendor module 自动解析
4. push 制品；以 `kh_consistency_check=1` 加载 `kernelhook.ko`
5. 逐 consumer 验证 —— `ksyms_lookup` 在最前（不挂 hook、零 spam），然后 4 个挂 hook 的 —— marker 检查在 **同一个 adb shell** 里执行（紧贴 loader 返回，避开多 `[permanent]` hook spam 引发的 dmesg 驱逐 race）
6. `hello_hook` PASS 时 snapshot dmesg 里的 `[kh_strategy]` 行 → 写 `tests/golden/strategy_matrix/values/<avd>.yaml.new`（仅 drift；手动 mv 接受）

`test_avd_graft.sh` 默认只跑 `hello_hook`（用 `--consumers=` 扩展）；如果 `make module-dual` 清掉了 `kh_payload.o` 会自动 rebuild。

## Loader 变体自动选择

`kmod_loader` 探测活体 kernel 的 ksymtab stride，挑对应的 `kernelhook-*.ko`：

| ksymtab stride | 变体                | 适用 kernel                          |
|----------------|---------------------|-------------------------------------|
| 12 B（PREL32） | `prel32`            | 5.10+ GKI                           |
| 24 B           | `abs64`             | 5.3..5.7（带 namespace 字段）       |
| 16 B（8 B CRC）| `abs64-legacy`      | 4.4（Android 9）                    |
| 16 B（4 B CRC）| `abs64-legacy-u32`  | 4.14 backport（Android 10）         |

CMODEL：所有变体强制 `-mcmodel=large`（仅 MOVW —— pre-GKI 4.x module loader 拒绝 `adrp`）。

## 已知限制

| Kernel / AVD                       | 状态     | 备注                                                                                              |
|------------------------------------|----------|---------------------------------------------------------------------------------------------------|
| 3.18（Pixel_27）                   | 跳过     | 早于 `abs64-legacy`；大模块 reloc 阶段挂死                                                         |
| API 36 baseline                    | 用 36.1  | `Pixel_36` 系统镜像在 macOS arm64 启不来；`Pixel_36_1` 覆盖该 API                                  |
| Pixel 6 实机 / 6.1.99-android14-11 | 未通过   | 自建 kmod 加载触发 hardlockup-watchdog → reboot。先试 graft 路径再改 kernelhook 源（见 `memory/project_pixel6_61_99_panic.md`）|

## 创建 AVD

```bash
sdkmanager "system-images;android-31;google_apis;arm64-v8a"
avdmanager create avd -n Pixel_31 -k "system-images;android-31;google_apis;arm64-v8a" -d pixel
```
