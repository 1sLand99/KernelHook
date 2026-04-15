# API 参考

头文件：`<kh_hook.h>`、`<symbol.h>`

## 使用方式

KernelHook 公共 API 有三种使用方式：

1. **SDK（推荐）** —— 在目标设备上加载 `kernelhook.ko`，然后加载你自己的
   消费者 `.ko`（通过 `KH_DECLARE_VERSIONS()` 引用 API 符号）。内核通过
   `kernelhook.ko` 导出的 ksymtab/kcrctab 解析这些符号。
   *本仓库所有示例的默认方式。*
2. **Freestanding 回退** —— 将核心库直接链接进你的 `.ko`。当目标机器没有
   `kernelhook.ko` 时使用（headerless 或受限内核）。如果
   [`docs/zh/freestanding.md`](freestanding.md) 存在可参考该文档；
   否则参考 `examples/<name>/Makefile.freestanding`。
3. **Kbuild 独立构建** —— 基于完整内核源码的 out-of-tree 构建。见
   [`examples/kbuild_hello/`](../../examples/kbuild_hello/)。

三种模式的符号可用性完全一致；只是链接路径不同。

## 内联 Hook

### `kh_hook`

```c
kh_hook_err_t kh_hook(void *func, void *replace, void **backup);
```

将 `func` 替换为 `replace`。原始入口点（重定位后的指令）保存在 `*backup` 中，可用于调用原函数。

- `func` -- 目标内核函数地址
- `replace` -- 替换函数（签名必须与原函数一致）
- `backup` -- 接收指向重定位后原始代码的指针

成功返回 `HOOK_NO_ERR`。

### `kh_unhook`

```c
void kh_unhook(void *func);
```

移除由 `kh_hook()` 安装的内联 kh_hook，恢复 `func` 处的原始指令。

## Hook 链（Wrap API）

Wrap API 支持在同一函数上注册多个 before/after 回调，按优先级排序执行。优先级数值越小，优先级越高，越先执行。

### `kh_hook_wrap`

```c
kh_hook_err_t kh_hook_wrap(void *func, int32_t argno, void *before, void *after,
                     void *udata, int32_t priority);
```

在 `func` 上注册一对 before/after 回调。

- `func` -- 目标函数地址
- `argno` -- 需要捕获的参数个数（0-12）
- `before` -- 原函数执行前调用（可为 NULL）
- `after` -- 原函数执行后调用（可为 NULL）
- `udata` -- 传递给回调的用户数据
- `priority` -- 执行顺序（越小越优先，0 = 最高优先级）

对同一 `func` 多次调用 `kh_hook_wrap` 会向链中添加回调（上限 `HOOK_CHAIN_NUM` = 8）。

### `kh_hook_wrap0` ... `kh_hook_wrap12`

```c
static inline kh_hook_err_t kh_hook_wrap4(void *func,
    kh_hook_chain4_callback before,
    kh_hook_chain4_callback after,
    void *udata);
```

类型安全的便捷封装，优先级默认为 0。后缀数字决定回调中使用的 `hook_fargsN_t` 类型。

### `kh_hook_unwrap`

```c
void kh_hook_unwrap(void *func, void *before, void *after);
```

从链中移除指定的 before/after 回调对。若链变空，kh_hook 会被完全移除。

### `wrap_get_origin_func`

```c
void *wrap_get_origin_func(void *hook_args);
```

在回调中获取重定位后的原函数指针。将 `fargs` 参数强制转换传入即可。

## 函数指针 Hook

Hook 存储在内存地址处的函数指针（例如 `struct` ops 表中的回调）。

### `kh_fp_hook`

```c
void kh_fp_hook(uintptr_t fp_addr, void *replace, void **backup);
```

将 `fp_addr` 处的函数指针替换为 `replace`，原始指针保存到 `*backup`。

### `kh_fp_unhook`

```c
void kh_fp_unhook(uintptr_t fp_addr, void *backup);
```

恢复 `fp_addr` 处的原始函数指针。

### `kh_fp_hook_wrap`

```c
kh_hook_err_t kh_fp_hook_wrap(uintptr_t fp_addr, int32_t argno,
                        void *before, void *after,
                        void *udata, int32_t priority);
```

类似 `kh_hook_wrap`，但作用于函数指针。最多支持 `FP_HOOK_CHAIN_NUM` = 16 个回调。

### `kh_fp_hook_unwrap`

```c
void kh_fp_hook_unwrap(uintptr_t fp_addr, void *before, void *after);
```

从函数指针 kh_hook 链中移除回调对。

### `fp_get_origin_func`

```c
void *fp_get_origin_func(void *hook_args);
```

在函数指针 kh_hook 回调中获取原始函数指针。

### `kh_fp_hook_wrap0` ... `kh_fp_hook_wrap12`

类型安全的便捷封装，与 `hook_wrapN` 用法类似。

## 系统调用 Hook

头文件：`<syscall.h>`

基于 `kh_hook_wrap` / `kh_fp_hook_wrap` 的高层 API，按系统调用号定位入口。自动检测 ARM64 `pt_regs` 包装器 ABI（内核 ≥ 4.17），定位到 `__arm64_sys_<name>`。

**设计说明**：GKI ≥ 5.10 内核的 `sys_call_table` 是 `__ro_after_init`，并且 kCFI 校验通过它的间接调用 —— 双重封锁。`kh_hook_syscalln` 特意总是走 inline kh_hook 直接 kh_hook `__arm64_sys_<name>`。`kh_sys_call_table` 只作诊断/发现用途保留。

### `kh_syscall_init`

```c
int kh_syscall_init(void);
```

通过 kallsyms 解析 `sys_call_table`（诊断用）并检测包装器 ABI。在子系统初始化阶段调用。成功返回 0。

### `kh_hook_syscalln`

```c
kh_hook_err_t kh_hook_syscalln(int nr, int narg, void *before, void *after, void *udata);
```

在系统调用号 `nr` 上安装 kh_hook。`narg` 是目标系统调用的逻辑参数数（例如 `__NR_execve` 为 3）。在包装器内核上物理回调仍然收到 `fargs->arg0` 中的 `pt_regs *`；用 `kh_syscall_argn_p` 访问第 N 个系统调用参数即可屏蔽 ABI 差异。

### `kh_unhook_syscalln`

```c
void kh_unhook_syscalln(int nr, void *before, void *after);
```

对称移除。**必须**在模块退出时调用，避免 inline-kh_hook 跳板指向已释放的模块 text。

### `kh_syscall_argn_p`

```c
#define kh_syscall_argn_p(args, N) /* ... */
```

宏。返回指向第 N 个系统调用参数的 `void *`（包装器 ABI 下指向 `pt_regs->regs[N]`，直调 ABI 下指向 `&args->argN`）。可写 —— 在此改写参数在内核继续 syscall 入口处理之前生效。

### `kh_raw_syscall0` ... `kh_raw_syscall6`

```c
long kh_raw_syscall0(long nr);
long kh_raw_syscall1(long nr, long a0);
/* ... 直到 kh_raw_syscall6 ... */
```

从内核上下文发起系统调用。处理包装器 ABI（合成伪 `pt_regs`）。用于模块内部触发已挂载的 syscall kh_hook 做测试 / 探测。

### 全局变量

```c
extern uintptr_t *kh_sys_call_table;    /* kallsyms 解析，诊断用 */
extern int        kh_has_syscall_wrapper; /* 内核 ≥ 4.17 含 pt_regs 包装时为 1 */
```

## 用户指针辅助

头文件：`<uaccess.h>`

面向 syscall kh_hook 的最小工具集，读取 / 改写 userspace 字符串（比如 `execve` 的 filename）。使用前需调用 `kh_uaccess_init()`。

### `kh_uaccess_init`

```c
int kh_uaccess_init(void);
```

通过 kallsyms 解析 `strncpy_from_user` / `copy_to_user`，并扫描 `init_task` 探测 `task_struct.cred` 偏移。成功返回 0。探测失败时 `kh_current_uid()` 返回 0（安全默认值）。

### `kh_strncpy_from_user`

```c
long kh_strncpy_from_user(char *dest, const void __user *src, long count);
```

从 userspace 拷贝以 NUL 结尾的字符串。成功时返回拷贝字节数（含终止符），错误时 <0。`dest` 成功时总是 NUL 终止。

### `kh_copy_to_user`

```c
int kh_copy_to_user(void __user *to, const void *from, int n);
```

从内核向用户态拷贝 `n` 字节。返回未拷贝的字节数（内核惯例：0 = 完全成功）。

### `kh_copy_to_user_stack`

```c
void __user *kh_copy_to_user_stack(const void *data, int len);
```

把 `data`（长 `len` 字节）写入当前任务的用户栈 `SP - aligned(len)` 位置，返回对应的用户指针。调用方必须运行在 `current` 具有有效用户 mm 的进程上下文（syscall kh_hook 满足）。是改写 `execve` 类 syscall 参数的关键原语。

### `kh_current_uid`

```c
uid_t kh_current_uid(void);
```

通过探测到的 `task_struct` 偏移读取 `current->cred->uid`。探测失败时返回 0（安全默认 —— 调用方把"未知"当作非特权对待仍然正确）。

## 符号解析

头文件：`<symbol.h>`

### `ksyms_init`

```c
int ksyms_init(uint64_t kallsyms_lookup_name_addr);
```

用内核 `kallsyms_lookup_name` 的运行时地址初始化符号解析器。必须在调用
`ksyms_lookup` 之前调用。成功返回 0，失败返回非零。

### `ksyms_lookup`

```c
uint64_t ksyms_lookup(const char *name);
```

按名称查找内核符号，返回地址。未找到时返回 0。必须先调用 `ksyms_init()`。

## 类型

### `hook_fargsN_t`

回调参数结构体，公共字段：

| 字段 | 类型 | 说明 |
|------|------|------|
| `chain` | `void *` | 内部使用——指向链状态 |
| `skip_origin` | `int` | 在 `before` 中置 1 可跳过原函数 |
| `local` | `kh_hook_local_t *` | 每回调本地存储（4 x uint64_t） |
| `ret` | `uint64_t` | 返回值（`after` 中读取，写入可覆盖） |
| `arg0`...`argN` | `uint64_t` | 函数参数（可读写） |

变体：`kh_hook_fargs0_t`（无参数）到 `kh_hook_fargs12_t`（12 个参数）。

- `kh_hook_fargs1_t` 到 `kh_hook_fargs3_t` 是 `kh_hook_fargs4_t` 的别名
- `kh_hook_fargs5_t` 到 `kh_hook_fargs7_t` 是 `kh_hook_fargs8_t` 的别名
- `kh_hook_fargs9_t` 到 `kh_hook_fargs11_t` 是 `kh_hook_fargs12_t` 的别名

### `kh_hook_local_t`

每回调本地存储，通过 `fargs->local->data0` 到 `data3`（或 `data[0..3]`）访问。链中每个回调拥有独立的 `kh_hook_local_t`。

### `kh_hook_err_t`

| 值 | 名称 | 说明 |
|----|------|------|
| 0 | `HOOK_NO_ERR` | 成功 |
| 4095 | `HOOK_BAD_ADDRESS` | 函数地址无效 |
| 4094 | `HOOK_DUPLICATED` | 该地址已存在 kh_hook |
| 4093 | `HOOK_NO_MEM` | 内存分配失败 |
| 4092 | `HOOK_BAD_RELO` | 指令重定位失败 |
| 4091 | `HOOK_TRANSIT_NO_MEM` | 中转桩分配失败 |
| 4090 | `HOOK_CHAIN_FULL` | 链已满（内联最多 8 / 函数指针最多 16） |

## 回调签名

```c
typedef void (*hook_chainN_callback)(hook_fargsN_t *fargs, void *udata);
```

N 为 0-12。以 4 参数函数为例：

```c
void my_before(kh_hook_fargs4_t *fargs, void *udata)
{
    uint64_t arg0 = fargs->arg0;       /* 读取参数 */
    fargs->arg1 = 0;                   /* 修改参数 */
    fargs->skip_origin = 1;            /* 跳过原函数 */
    fargs->ret = -EPERM;               /* 设置返回值 */
    fargs->local->data0 = arg0;        /* 保存到本地存储 */
}

void my_after(kh_hook_fargs4_t *fargs, void *udata)
{
    uint64_t saved = fargs->local->data0;  /* 读取本地存储 */
    fargs->ret = 0;                        /* 覆盖返回值 */
}
```
