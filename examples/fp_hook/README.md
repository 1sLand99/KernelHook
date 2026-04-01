# fp_hook

Hook a function pointer in a struct, call original via backup, then unhook.

Hook 结构体中的函数指针，通过备份指针调用原函数，最后恢复。

## API

- `fp_hook` -- replace function pointer at a memory address
- `fp_unhook` -- restore original function pointer

## Build / 构建

```bash
# Mode A (Freestanding)
make module

# Mode B (SDK — requires kernelhook.ko loaded first)
make -f Makefile.sdk module

# Mode C (Kbuild — requires kernel source)
make -C /path/to/kernel M=$(pwd) modules
```

## Load / 加载

```bash
kmod_loader fp_hook.ko kallsyms_addr=0x...
```

## Expected dmesg / 预期输出

```
fp_hook: before hook: ops.callback(3,4) = 7
fp_hook: replacement called with x=3 y=4
fp_hook: original returned 7, we return 12
fp_hook: after hook: ops.callback(3,4) = 12
```
