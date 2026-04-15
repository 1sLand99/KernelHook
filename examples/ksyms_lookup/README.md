# ksyms_lookup

Runtime kernel symbol resolution. Look up multiple symbols and handle nonexistent symbols.

运行时内核符号解析。查找多个符号、处理不存在的符号。

## API

- `ksyms_lookup` -- look up kernel symbol by name, returns address (0 if not found)

## Build (SDK mode — default)

Requires `kernelhook.ko` to be loaded on the target device first.

```sh
make module                            # produces ksyms_lookup.ko
```

## Load + Run

```sh
adb push ../../kmod/kernelhook.ko /data/local/tmp/
adb push ksyms_lookup.ko          /data/local/tmp/
adb shell su -c '/data/local/tmp/kmod_loader /data/local/tmp/kernelhook.ko'
adb shell su -c '/data/local/tmp/kmod_loader /data/local/tmp/ksyms_lookup.ko'
adb shell su -c 'dmesg | tail -20'
```

## Expected dmesg / 预期输出

```
ksyms_lookup: vfs_read = ffffffc0xxxxxxxx
ksyms_lookup: vfs_write = ffffffc0xxxxxxxx
ksyms_lookup: do_sys_openat2 = ffffffc0xxxxxxxx
ksyms_lookup: vfs_read = ffffffc0xxxxxxxx
ksyms_lookup: nonexistent symbol = 0 (expected 0)
ksyms_lookup: all lookups complete
```

## Notes / 备注

This example only needs `kmod_compat_init` -- no `kh_mem_init` or `pgtable_init` required since it does not install any hooks.

本示例只需要 `kmod_compat_init`，不安装任何 hook，因此无需 `kh_mem_init` 或 `pgtable_init`。

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
