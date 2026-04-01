# KernelHook

面向 Linux 内核的 ARM64 函数 hook 框架。

## 功能特性

- **内联 hook** -- 替换任意内核函数，通过备份指针调用原函数
- **Hook 链** -- 同一函数上注册多个 before/after 回调，按优先级排序执行
- **函数指针 hook** -- hook ops 表中的回调函数，支持链式调用
- **符号解析** -- `ksyms_lookup` / `ksyms_lookup_cache` 运行时查找内核符号
- **三种构建模式** -- Freestanding（无需内核头文件）、SDK（共享 kernelhook.ko）、Kbuild（标准方式）
- **自适应加载器** -- `kmod_loader` 修补 .ko 二进制文件，实现跨内核版本加载

## 快速开始

```bash
# 构建 hello_hook 示例（模式 A，freestanding）
cd examples/hello_hook
make module

# 构建自适应加载器
cd ../../tools/kmod_loader
make

# 推送到设备
adb push kmod_loader hello_hook.ko /data/local/tmp/

# 获取 kallsyms_lookup_name 地址
ADDR=$(adb shell "su -c 'cat /proc/kallsyms'" | awk '/kallsyms_lookup_name$/{print "0x"$1}')

# 加载
adb shell "su -c '/data/local/tmp/kmod_loader /data/local/tmp/hello_hook.ko kallsyms_addr=$ADDR'"

# 验证
adb shell dmesg | grep hello_hook
```

## 架构

| 组件 | 说明 |
|------|------|
| `src/arch/arm64/inline.c` | 指令重定位引擎 + 代码修补 |
| `src/arch/arm64/transit.c` | 中转桩 + 回调分发 |
| `src/arch/arm64/pgtable.c` | 页表遍历 + PTE 修改 |
| `src/core_user.c` | Hook 链 API（hook/unhook/hook_wrap） |
| `src/hmem.c` | ROX/RW 内存池的位图分配器 |
| `kmod/` | SDK、链接脚本、shim 头文件 |
| `tools/kmod_loader/` | 自适应模块加载器 |
| `examples/` | hello_hook、fp_hook、hook_chain、hook_wrap_args、ksyms_lookup |

## 文档

- [快速上手](docs/zh/getting-started.md)
- [构建模式](docs/zh/build-modes.md)
- [API 参考](docs/zh/api-reference.md)
- [kmod_loader](docs/zh/kmod-loader.md)
- [示例](docs/zh/examples.md)

[English](README.md)

## 构建与测试

### 用户态测试（macOS / Android）

```bash
# macOS（Apple Silicon）
cmake -B build_debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build_debug
cd build_debug && ctest

# Android（交叉编译）
cmake -B build_android -DCMAKE_TOOLCHAIN_FILE=cmake/android-arm64.cmake -DCMAKE_BUILD_TYPE=Debug
cmake --build build_android
adb push build_android/tests/test_* /data/local/tmp/
adb shell /data/local/tmp/test_hook_basic
```

### 内核模块

```bash
# 模式 A（freestanding，无需内核头文件）
cd examples/hello_hook && make module

# 模式 B（SDK，依赖 kernelhook.ko）
cd examples/hello_hook && make -f Makefile.sdk module

# 模式 C（Kbuild，需要内核源码）
cd examples/hello_hook && make -C /path/to/kernel M=$(pwd) modules
```

## 许可证

GPL-2.0-or-later
