# kmod_loader

Adaptive kernel module loader. Patches freestanding `.ko` binaries at load time to match the running kernel's ABI.

自适应内核模块加载器。在加载时修补 freestanding `.ko` 二进制文件，使其匹配当前内核的 ABI。

## Usage / 用法

```
kmod_loader <module.ko> [kallsyms_addr=0xHEX] [options] [param=value ...]
```

## Options / 选项

| Option | Description |
|--------|-------------|
| `kallsyms_addr=0xHEX` | Address of `kallsyms_lookup_name` (required) |
| `--init-off 0xHEX` | Override struct module init offset |
| `--exit-off 0xHEX` | Override struct module exit offset |
| `--probe` | Force re-probe init/exit offsets |
| `--crc sym=0xHEX` | Override CRC for symbol (repeatable) |

## Build / 构建

```bash
make              # Build kmod_loader (host binary)
make regenerate   # Rebuild embedded probe_module.ko (needs cross-compiler)
make clean        # Clean build artifacts
```

The embedded `probe_module_embed.h` is pre-committed -- `make` only requires a host C compiler.

内嵌的 `probe_module_embed.h` 已预先提交，`make` 只需宿主机 C 编译器。

## What It Patches / 修补内容

- **vermagic** -- matches running kernel
- **CRC values** -- `module_layout`, `printk`, etc.
- **init/exit offsets** -- struct module layout differences
- **struct module size** -- kernel version differences
- **printk symbol name** -- `_printk` (6.1+) vs `printk` (5.10)
- **kallsyms_addr** -- patched directly into ELF .data section

## Example / 示例

```bash
# Physical device
ADDR=$(adb shell "su -c 'cat /proc/kallsyms'" | awk '/kallsyms_lookup_name$/{print "0x"$1}')
adb push kmod_loader my_hook.ko /data/local/tmp/
adb shell "su -c '/data/local/tmp/kmod_loader /data/local/tmp/my_hook.ko kallsyms_addr=$ADDR'"

# AVD (extract CRCs from host kernel image)
CRC_ARGS=$(python3 ../../scripts/extract_avd_crcs.py -s emulator-5554)
adb shell "/data/local/tmp/kmod_loader /data/local/tmp/my_hook.ko kallsyms_addr=$ADDR $CRC_ARGS"
```

## Documentation / 文档

- [English](../../docs/en/kmod-loader.md)
- [中文](../../docs/zh/kmod-loader.md)
