# AVD 内核模块测试

在 Android 虚拟设备模拟器上自动化运行 KernelHook 内核模块测试的基础设施。

## 快速开始

```bash
# 测试所有支持的 AVD
./scripts/test_avd_kmod.sh

# 测试指定 AVD
./scripts/test_avd_kmod.sh Pixel_31 Pixel_34 Pixel_37

# 在运行中的模拟器上跑用户态测试套件（自动 adb root + setenforce 0）
./scripts/run_android_tests.sh --serial emulator-5554 --build-dir build_android

# 手动单设备 kmod 测试（仅 USB / magisk 真机）
./scripts/run_android_tests.sh --kmod
```

> `run_android_tests.sh` 在 userdebug 模拟器上会自动执行 `adb root` +
> `setenforce 0`，让用户态测试可以 `mprotect RW→RX`。`--kmod` 路径仍然依赖
> `su -c` helper，仅适用于 USB / magisk 真机；模拟器上跑 kmod 回归请使用上面
> 的 `test_avd_kmod.sh`。

## 前置要求

- Android SDK（含模拟器和系统镜像，API 28+）
- Android NDK r29+（自动从 `~/Library/Android/sdk/ndk/` 检测）
- Python 3（用于 CRC 提取）
- 已创建目标 API 级别的 AVD（如 `Pixel_31`、`Pixel_37`）

## 测试流程

`test_avd_kmod.sh` 对每个 AVD 执行以下步骤：

1. **杀掉已有模拟器**并启动目标 AVD
2. **等待启动完成**并获取 `adb root`
3. **构建** `kh_test.ko`，使用内核版本特定的编译参数
4. **提取 CRC**，从宿主机的 `kernel-ranchu` 镜像
5. **推送** `kh_test.ko` 和 `kmod_loader` 到设备
6. **加载**模块，通过 `kmod_loader` 传入提取的 CRC
7. **捕获** dmesg 中的测试结果（每个内核 25 个测试）
8. **卸载**模块并杀掉模拟器

## 构建配置

`tests/kmod/Makefile` 按内核版本自动配置：

| 特性 | 条件 | 效果 |
|------|------|------|
| `-mcmodel=large` | 内核 <= 4.x | 避免 4.x 模块加载器不支持的 ADRP 重定位 |
| `-fsanitize=kcfi` | 内核 >= 6.1 | 为 6.1+ 内核启用 kCFI 元数据 |
| `THIS_MODULE_SIZE=0x800` | 所有版本 | 填充到 0x800，`kmod_loader` 运行时 shrink 到实际设备大小 |
| `MODULE_INIT_OFFSET` | 按版本 | GKI 预设值；`kmod_loader` 通过 vendor 内省覆盖 |

## CRC 提取

`scripts/extract_avd_crcs.py` 从宿主机的 `kernel-ranchu` 镜像中提取符号 CRC。自动检测三种 ksymtab 条目格式：

| 格式 | 大小 | 适用内核 | CRC 大小 |
|------|------|---------|----------|
| prel32 | 12B | 5.10+ | 4B |
| 绝对指针 | 16B | 4.x | 4B 或 8B |
| 绝对指针+CFI | 24B | 5.4 | 4B |

对于未重定位的内核镜像（4.14 及更早版本），工具会回退到通过 `/proc/kallsyms` 中的 `__ksymtab_<sym>` 地址查找。

```bash
# 独立使用
python3 scripts/extract_avd_crcs.py -s emulator-5554 module_layout printk memcpy memset
# 输出: --crc module_layout=0x7c24b32d --crc printk=0xc5850110 ...
```

## 已知限制

| 内核版本 | 问题 | 根因 |
|---------|------|------|
| 3.18 (API 25-27) | 跳过 | 大模块加载挂起（MOVW 重定位开销过大） |
| 5.4 (API 30) | 已修复 | 原因：`_error_injection_whitelist` 段溢出 + `exit_off` 预设错误（0x350→0x340） |
| Pixel_36 | 启动超时 | AVD 系统镜像无法启动；API 36 由 Pixel_36.1 覆盖 |

## 创建测试用 AVD

```bash
# 安装系统镜像
sdkmanager "system-images;android-31;google_apis;arm64-v8a"
sdkmanager "system-images;android-37.0;google_apis_ps16k;arm64-v8a"

# 创建 AVD
avdmanager create avd -n Pixel_31 -k "system-images;android-31;google_apis;arm64-v8a" -d pixel
avdmanager create avd -n Pixel_37 -k "system-images;android-37.0;google_apis_ps16k;arm64-v8a" -d pixel
```
