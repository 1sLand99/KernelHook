# Contributing Device Profiles / 贡献设备配置文件

## What are device profiles? / 什么是设备配置文件？

Each `.conf` file in `kmod/devices/` describes the ABI-specific values for one
kernel build: CRC checksums for key exported symbols, `struct module` layout
offsets, and the vermagic string. `kmod_loader` uses these at runtime to patch
kernel modules so that a single `.ko` binary can load across many supported
Android kernels.

每个 `kmod/devices/` 目录下的 `.conf` 文件描述了某个内核构建的 ABI 特定值：
关键导出符号的 CRC 校验和、`struct module` 布局偏移量以及 vermagic 字符串。
`kmod_loader` 在运行时使用这些值来修补内核模块，使单个 `.ko` 二进制文件能够在
多个受支持的 Android 内核上加载。

## How to extract CRC data / 如何提取 CRC 数据

### Using the AVD extraction script / 使用 AVD 提取脚本

The easiest method for emulator-based kernels:

```bash
# Boot an AVD, then:
python3 scripts/extract_avd_crcs.py
```

The script connects via ADB, reads `/proc/modules` and symbol CRCs from the
running kernel, and outputs a `.conf`-format template.

对于模拟器内核，最简单的方法是启动 AVD 后运行上述脚本。脚本会通过 ADB 连接，
从运行中的内核读取 `/proc/modules` 和符号 CRC，然后输出 `.conf` 格式的模板。

### Using kmod_loader probe / 使用 kmod_loader probe

For physical devices with ADB access:

```bash
adb shell
kmod_loader probe --verbose
```

This captures all discoverable values directly from the device.

对于有 ADB 访问权限的物理设备，可以使用 `kmod_loader probe --verbose` 直接
从设备捕获所有可发现的值。

## Required fields / 必填字段

| Field | Section | Meaning |
|---|---|---|
| `name` | `[identity]` | Unique snake_case identifier, matches filename (without `.conf`, dots replaced by underscores) |
| `class` | `[identity]` | Profile classification: `gki`, `aosp`, or `vendor` (see below) |
| `arch` | `[identity]` | CPU architecture, currently always `aarch64` |
| `kernelrelease` | `[match]` | Prefix or exact string matched against `uname -r` |
| `module_layout` | `[modversions]` | CRC of `module_layout` symbol |
| `_printk` | `[modversions]` | CRC of `_printk` symbol |
| `memcpy` | `[modversions]` | CRC of `memcpy` symbol |
| `memset` | `[modversions]` | CRC of `memset` symbol |
| `this_module_size` | `[struct_module]` | `sizeof(struct module)` for this kernel |
| `module_init_offset` | `[struct_module]` | `offsetof(struct module, init)` |
| `module_exit_offset` | `[struct_module]` | `offsetof(struct module, exit)` |
| `string` | `[vermagic]` | Full vermagic string from the kernel |

Optional: `[provenance]` section with `verified`, `source_module`, `extracted_by`,
`extracted_at`, `contributed_by`.

可选字段：`[provenance]` 部分，包含 `verified`、`source_module`、`extracted_by`、
`extracted_at`、`contributed_by`。

## Classification rules / 分类规则

The `class` field in `[identity]` classifies the kernel build:

`[identity]` 中的 `class` 字段对内核构建进行分类：

### `gki` — Generic Kernel Image (5.4+)

- Kernels 5.4 and above built from the AOSP GKI source.
- `kernelrelease` uses **prefix match** (e.g., `5.4.`, `6.1.`).
- Filename prefix: `gki_`.

适用于从 AOSP GKI 源码构建的 5.4 及以上版本内核。`kernelrelease` 使用前缀匹配。

### `aosp` — Pre-GKI AOSP common kernels

- Kernels 4.4, 4.9, 4.14, 4.19 built from AOSP common kernel source (before GKI was introduced).
- `kernelrelease` uses **prefix match** (e.g., `4.14.`).
- Filename prefix: `aosp_`.

适用于 GKI 引入之前从 AOSP 通用内核源码构建的 4.4、4.9、4.14、4.19 版本内核。
`kernelrelease` 使用前缀匹配。

### `vendor` — OEM-specific builds

- Kernels with OEM-specific modifications (Samsung, Xiaomi, OnePlus, etc.).
- `kernelrelease` uses **exact `uname -r` match** because OEM builds diverge from AOSP.
- Filename prefix: `vendor_`.

适用于有 OEM 特定修改的内核（三星、小米、一加等）。`kernelrelease` 使用
精确的 `uname -r` 匹配，因为 OEM 构建与 AOSP 存在差异。

## Submitting a pull request / 提交 Pull Request

1. Extract CRC data from the target device using one of the methods above.
2. Create a new `.conf` file following the naming convention:
   - `gki_<major>.<minor>_<android_version>.conf` for GKI kernels
   - `aosp_<major>.<minor>_<android_version>.conf` for pre-GKI AOSP kernels
   - `vendor_<oem>_<device>_<kernel_version>.conf` for vendor kernels
3. Validate against `schema.json`:
   ```bash
   python3 -c "import configparser, json; ..."  # or use your preferred validator
   ```
4. Regenerate the C table:
   ```bash
   python3 tools/gen_devices_table.py \
       --devices-dir kmod/devices \
       --output tools/kmod_loader/devices_table.generated.c
   ```
5. Open a PR. CI will lint the file against `schema.json`, run unit tests, and
   if an AVD image exists for the target kernel, run an end-to-end load test.
6. A committer reviews and merges.

---

1. 使用上述方法之一从目标设备提取 CRC 数据。
2. 按照命名规范创建新的 `.conf` 文件（参见上方英文命名示例）。
3. 对照 `schema.json` 进行验证。
4. 重新生成 C 表（运行 `tools/gen_devices_table.py`）。
5. 提交 PR。CI 会自动进行 schema 校验、单元测试，如果目标内核有 AVD 镜像，
   还会进行端到端加载测试。
6. 维护者审核后合并。
