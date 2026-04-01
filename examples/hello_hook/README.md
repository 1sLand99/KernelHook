# hello_hook

Hook `do_sys_openat2` and log the filename pointer on every `open()` syscall.

Hook `do_sys_openat2`，记录每次 `open()` 系统调用的文件名指针。

## API

- `hook_wrap4` -- register a before callback capturing 4 arguments
- `hook_unwrap` -- remove the callback on module exit

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
# With kmod_loader (cross-kernel)
kmod_loader hello_hook.ko kallsyms_addr=0x...

# With insmod (matching kernel)
insmod hello_hook.ko kallsyms_addr=0x...
```

## Expected dmesg / 预期输出

```
hello_hook: hooked do_sys_open* at ffffffc0xxxxxxxx
hello_hook: open called, filename ptr=7fxxxxxxxx
```
