# hello_hook

Hook `do_sys_openat2` and log the filename pointer on every `open()` syscall.

Hook `do_sys_openat2`，记录每次 `open()` 系统调用的文件名指针。

## API

- `kh_hook_wrap4` -- register a before callback capturing 4 arguments
- `kh_hook_unwrap` -- remove the callback on module exit

## Build (SDK mode — default)

Requires `kernelhook.ko` to be loaded on the target device first.

```sh
make module                            # produces hello_hook.ko
```

## Load + Run

```sh
adb push ../../kmod/kernelhook.ko /data/local/tmp/
adb push hello_hook.ko             /data/local/tmp/
adb shell su -c '/data/local/tmp/kmod_loader /data/local/tmp/kernelhook.ko'
adb shell su -c '/data/local/tmp/kmod_loader /data/local/tmp/hello_hook.ko'
adb shell su -c 'dmesg | tail -20'
```

## Expected dmesg / 预期输出

```
hello_hook: hooked do_sys_open* at ffffffc0xxxxxxxx
hello_hook: open called, filename ptr=7fxxxxxxxx
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
