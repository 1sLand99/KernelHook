# hook_chain

Register multiple before/after callbacks on `do_sys_openat2` with different priorities. Demonstrates that priority controls execution order, not registration order.

在 `do_sys_openat2` 上注册多个不同优先级的 before/after 回调。展示优先级决定执行顺序，而非注册顺序。

## API

- `hook_wrap` with `priority` parameter -- lower number = higher priority = runs first
- `hook_unwrap` -- remove each callback pair individually

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
kmod_loader hook_chain.ko kallsyms_addr=0x...
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
