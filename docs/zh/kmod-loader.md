# kmod_loader -- 自适应模块加载器

`kmod_loader` 是一个用户态工具，在加载时动态修补 freestanding `.ko` 二进制文件，使其适配当前运行内核的 ABI，实现跨内核版本加载，无需重新编译。

## 用法

```
kmod_loader <module.ko> [kallsyms_addr=0xHEX] [选项] [param=value ...]
```

## 修补内容

| 字段 | 方式 |
|------|------|
| **vermagic** | 替换为当前内核的 `uname -r` + 标准后缀 |
| **CRC 值** | 修补 `__versions` 段中的 CRC，匹配 `module_layout`、`printk` 等符号 |
| **init/exit 偏移** | 调整 `.rela.gnu.linkonce.this_module` 中的重定位，匹配 `struct module` 布局 |
| **struct module 大小** | 调整 `.gnu.linkonce.this_module` 段大小 |
| **printk 符号名** | 自动检测 `_printk`（6.1+）或 `printk`（5.10），修改 `__versions` 和字符串表 |
| **kallsyms_addr** | 直接修补 ELF `.data` 段（绕过 `module_param` / shadow CFI 问题） |
| **this_module 段** | 修补前清零，防止 `ei_funcs`/`num_ei_funcs` 垃圾数据 |

## 命令行选项

| 选项 | 说明 |
|------|------|
| `kallsyms_addr=0xHEX` | `kallsyms_lookup_name` 的地址（大多数模块必需） |
| `--init-off 0xHEX` | 手动指定 `struct module` 中 init 函数的偏移 |
| `--exit-off 0xHEX` | 手动指定 `struct module` 中 exit 函数的偏移 |
| `--probe` | 强制重新探测 init/exit 偏移（忽略缓存） |
| `--crc sym=0xHEX` | 手动指定某个符号的 CRC（可重复使用） |
| `param=value` | 模块参数，透传给 `init_module` |

## init/exit 偏移解析策略

`kmod_loader` 采用分级策略查找 `struct module` 中正确的 `init` 和 `exit` 偏移：

1. **持久缓存** -- 之前探测的偏移存储在 `kmod_loader` 二进制自身中
2. **版本预设** -- 已知内核版本（4.4 至 6.12）的硬编码偏移
3. **方法 A（kcore 反汇编）** -- 从 `/proc/kcore` 读取 `do_init_module`，扫描 `LDR X0, [Xn, #imm]; CBZ X0; BL do_one_initcall` 指令模式
4. **方法 B（二进制探测）** -- 内嵌一个最小的 `probe_module.ko`（init 返回 `-EINVAL`），在 0x140-0x200 范围内以步长 8 尝试各候选偏移。返回 `EINVAL` 表示 init 被成功调用，即偏移正确。支持崩溃恢复。

使用 `--init-off` / `--exit-off` 可跳过自动检测。

## CRC 解析顺序

CRC 值按以下顺序解析：

1. `--crc` 命令行参数
2. kallsyms 中的 `__crc_*` 符号（来自 `/proc/kallsyms`）
3. 设备上的厂商 `.ko` 文件（解析 `__versions` 段）
4. Boot 分区的内核镜像
5. `finit_module` 的 `MODULE_INIT_IGNORE_MODVERSIONS` 标志（如果内核支持）

## AVD 专用说明

Android 虚拟设备没有 `/proc/kcore` 和厂商 `.ko` 文件，CRC 需要从宿主机的内核镜像中提取：

```bash
# 从 AVD 内核镜像提取 CRC
CRC_ARGS=$(python3 scripts/extract_avd_crcs.py -s emulator-5554)

# 使用提取的 CRC 加载
adb shell "/data/local/tmp/kmod_loader /data/local/tmp/my_hook.ko \
    kallsyms_addr=0x... $CRC_ARGS"
```

## 构建

```bash
cd tools/kmod_loader
make              # 构建 kmod_loader（宿主机二进制）
make regenerate   # 重新生成内嵌的 probe_module.ko（需要交叉编译器）
```

`probe_module_embed.h` 已预先提交，因此 `make` 只需要宿主机 C 编译器。

## 示例

```bash
# 物理设备（Pixel 6，内核 6.1）
ADDR=$(adb shell "su -c 'cat /proc/kallsyms'" | awk '/kallsyms_lookup_name$/{print "0x"$1}')
adb push kmod_loader hello_hook.ko /data/local/tmp/
adb shell "su -c '/data/local/tmp/kmod_loader /data/local/tmp/hello_hook.ko kallsyms_addr=$ADDR'"

# AVD（内核 5.10）
CRC_ARGS=$(python3 scripts/extract_avd_crcs.py -s emulator-5554)
adb shell "/data/local/tmp/kmod_loader /data/local/tmp/hello_hook.ko \
    kallsyms_addr=$ADDR $CRC_ARGS"
```
