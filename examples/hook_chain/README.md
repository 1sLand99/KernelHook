# hook_chain

Register multiple before/after callbacks on `do_sys_openat2` with different priorities. Demonstrates that priority controls execution order, not registration order.

在 `do_sys_openat2` 上注册多个不同优先级的 before/after 回调。展示优先级决定执行顺序，而非注册顺序。

## API

- `kh_hook_wrap` with `priority` parameter -- lower number = higher priority = runs first
- `kh_hook_unwrap` -- remove each callback pair individually

## Build (SDK mode — default)

Requires `kernelhook.ko` to be loaded on the target device first.

```sh
make module                            # produces hook_chain.ko
```

## Load + Run

```sh
adb push ../../kmod/kernelhook.ko /data/local/tmp/
adb push hook_chain.ko            /data/local/tmp/
adb shell su -c '/data/local/tmp/kmod_loader /data/local/tmp/kernelhook.ko'
adb shell su -c '/data/local/tmp/kmod_loader /data/local/tmp/hook_chain.ko'
adb shell su -c 'dmesg | tail -20'
```

## Expected dmesg / 预期输出

```
hook_chain: registered 3 before callbacks + 1 after callback
hook_chain: execution order will be: high(0) -> medium(50) -> low(100)
hook_chain: [priority 0] HIGH priority before callback
hook_chain: [priority 50] MEDIUM priority before callback
hook_chain: [priority 100] LOW priority before callback
hook_chain: after callback, ret=...
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
