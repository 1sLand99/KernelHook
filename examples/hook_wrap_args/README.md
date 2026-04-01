# hook_wrap_args

Hook `do_sys_openat2` with before and after callbacks. Inspect all arguments in before, read and override the return value in after.

用 before 和 after 回调 hook `do_sys_openat2`。在 before 中查看所有参数，在 after 中读取并覆盖返回值。

## API

- `hook_wrap4` -- register before + after callbacks for a 4-arg function
- `fargs->arg0` ... `fargs->arg3` -- read/write function arguments
- `fargs->ret` -- read original return value (in after), write to override

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
kmod_loader hook_wrap_args.ko kallsyms_addr=0x...
```

## Expected dmesg / 预期输出

```
hook_wrap_args: hooked do_sys_open* at ffffffc0xxxxxxxx
hook_wrap_args: BEFORE arg0(dfd)=... arg1(filename)=... arg2(how)=... arg3=...
hook_wrap_args: AFTER original ret=..., overriding with 0
```
