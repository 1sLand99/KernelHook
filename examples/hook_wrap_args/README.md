# hook_wrap_args

Hook `do_sys_openat2` with before and after callbacks. Inspect all arguments in before, read and override the return value in after.

用 before 和 after 回调 hook `do_sys_openat2`。在 before 中查看所有参数，在 after 中读取并覆盖返回值。

## API

- `kh_hook_wrap4` -- register before + after callbacks for a 4-arg function
- `fargs->arg0` ... `fargs->arg3` -- read/write function arguments
- `fargs->ret` -- read original return value (in after), write to override

## Build (SDK mode — default)

Requires `kernelhook.ko` to be loaded on the target device first.

```sh
make module                            # produces hook_wrap_args.ko
```

## Load + Run

```sh
adb push ../../kmod/kernelhook.ko /data/local/tmp/
adb push hook_wrap_args.ko        /data/local/tmp/
adb shell su -c '/data/local/tmp/kmod_loader /data/local/tmp/kernelhook.ko'
adb shell su -c '/data/local/tmp/kmod_loader /data/local/tmp/hook_wrap_args.ko'
adb shell su -c 'dmesg | tail -20'
```

## Expected dmesg / 预期输出

```
hook_wrap_args: hooked do_sys_open* at ffffffc0xxxxxxxx
hook_wrap_args: BEFORE arg0(dfd)=... arg1(filename)=... arg2(how)=... arg3=...
hook_wrap_args: AFTER original ret=..., overriding with 0
```

## Other modes

This example also supports two legacy build paths:

| Path                                    | Use when                                                                                     |
|-----------------------------------------|----------------------------------------------------------------------------------------------|
| `make -f Makefile.freestanding module`  | No `kernelhook.ko` on target — produces a self-contained `.ko` that links the core in.       |
| `make -C /path/to/kernel M=$(pwd)`*     | You have a full kernel source tree (uses `Kbuild.standalone`).                               |

\* Uses `Kbuild.standalone` after the P1 rename. Pass `KBUILD_EXTMOD_FILE=Kbuild.standalone` if your kbuild doesn't pick it up automatically.

## Migration (P1, 2026-04-15)

If you previously ran `make module` here and got a self-contained `.ko`
(freestanding mode), the default has changed: `make module` now builds
in **SDK mode** and requires `kernelhook.ko` on the target. The old
behaviour is still available — invoke `make -f Makefile.freestanding module`.
