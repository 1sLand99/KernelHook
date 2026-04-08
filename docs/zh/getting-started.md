# 快速上手

## 环境准备

- **模式 A（Freestanding）：** Android NDK 或 `aarch64-linux-gnu-` 交叉编译器，无需内核头文件。
- **模式 B（SDK）：** 同模式 A，但目标设备需先加载 `kernelhook.ko`。
- **模式 C（Kbuild）：** 需要目标内核的源码树或头文件。
- **目标设备：** ARM64 Linux 内核（Android 手机、AVD 模拟器或任意 aarch64 Linux）。

## 快速开始（模式 A）

克隆仓库并构建 `hello_hook` 示例：

```bash
git clone https://github.com/bmax121/KernelHook.git
cd KernelHook/examples/hello_hook
make module
```

生成的 `hello_hook.ko` 是一个独立内核模块，会 hook `do_sys_openat2` 并记录每次 `open()` 系统调用的文件名。

### 加载到设备

若 CRC/vermagic 与当前内核匹配，可直接用 `insmod`。`insmod` 不会自动获取符号
地址，需要显式传入 `kallsyms_addr=`：

```bash
adb push hello_hook.ko /data/local/tmp/
ADDR=$(adb shell "su -c 'grep \" kallsyms_lookup_name$\" /proc/kallsyms'" | awk '{print "0x"$1}')
adb shell "su -c 'insmod /data/local/tmp/hello_hook.ko kallsyms_addr=$ADDR'"
```

若 CRC/vermagic 不匹配（跨内核加载），使用 `kmod_loader`。loader 会自动从
`/proc/kallsyms` 获取 `kallsyms_lookup_name` 地址，无需手动传参：

```bash
cd ../../tools/kmod_loader
make
adb push kmod_loader hello_hook.ko /data/local/tmp/
adb shell "su -c '/data/local/tmp/kmod_loader /data/local/tmp/hello_hook.ko'"
# 若 kptr_restrict 对 root 屏蔽了符号地址，可追加 kallsyms_addr=0xHEX 覆盖。
```

### 验证

```bash
adb shell dmesg | grep hello_hook
# hello_hook: hooked do_sys_open* at ffffffc0xxxxxxxx
# hello_hook: open called, filename ptr=...
```

### 卸载

```bash
adb shell "su -c 'rmmod hello_hook'"
```

## 下一步

- [构建模式](build-modes.md) -- 模式 A / B / C 详细对比
- [API 参考](api-reference.md) -- 完整的 hook API 文档
- [示例](examples.md) -- 所有示例模块及代码详解
- [kmod_loader](kmod-loader.md) -- 自适应模块加载器参考
