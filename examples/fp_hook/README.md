# fp_hook

Hook a function pointer in a struct, call original via backup, then unhook.

Hook 结构体中的函数指针，通过备份指针调用原函数，最后恢复。

## API

- `fp_hook` -- replace function pointer at a memory address
- `fp_unhook` -- restore original function pointer

## Build (SDK mode — default)

Requires `kernelhook.ko` to be loaded on the target device first.

```sh
make module                            # produces fp_hook.ko
```

## Load + Run

```sh
adb push ../../kmod/kernelhook.ko /data/local/tmp/
adb push fp_hook.ko               /data/local/tmp/
adb shell su -c '/data/local/tmp/kmod_loader /data/local/tmp/kernelhook.ko'
adb shell su -c '/data/local/tmp/kmod_loader /data/local/tmp/fp_hook.ko'
adb shell su -c 'dmesg | tail -20'
```

## Expected dmesg / 预期输出

```
fp_hook: before hook: ops.callback(3,4) = 7
fp_hook: replacement called with x=3 y=4
fp_hook: original returned 7, we return 12
fp_hook: after hook: ops.callback(3,4) = 12
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
