# Build Modes

KernelHook supports three build modes for kernel modules. Choose based on your constraints.

## Comparison

| | Mode A (Freestanding) | Mode B (SDK) | Mode C (Kbuild) |
|---|---|---|---|
| Kernel headers | Not needed | Not needed | Required |
| kernelhook.ko | Not needed | Must load first | Optional |
| Core lib in .ko | Yes (compiled in) | No (linked at runtime) | Optional |
| Load method | insmod / kmod_loader | insmod / kmod_loader | insmod |
| .ko size | Large (~200KB) | Small (~10KB) | Depends |
| Best for | Single-module deployment | Multi-module deployment | Standard kernel dev |

## Mode A -- Freestanding

No kernel headers required. Uses `shim.h` as a minimal kernel header replacement. The core hooking library is compiled directly into your `.ko`.

### Build

```bash
cd examples/hello_hook
make module
```

The Makefile includes `kmod/mk/kmod.mk`, which handles cross-compilation, linker scripts, and the shim layer automatically.

### Makefile Template

```makefile
MODULE_NAME  := my_hook
MODULE_SRCS  := my_hook.c
KERNELHOOK_DIR := /path/to/KernelHook/kmod
include $(KERNELHOOK_DIR)/mk/kmod.mk
```

### Source Includes

```c
#include "../../kmod/shim/shim.h"
#include <ktypes.h>
#include <hook.h>
#include <ksyms.h>
#include <log.h>
#include <hmem.h>
#include <arch/arm64/pgtable.h>
#include "../../kmod/src/compat.h"
#include "../../kmod/src/mem_ops.h"
```

### Init Sequence

Mode A modules must initialize the subsystems manually:

```c
static int __init my_hook_init(void)
{
    int rc;

    rc = kmod_compat_init(kallsyms_addr);    /* symbol resolution */
    if (rc) return rc;

    rc = kmod_hook_mem_init();               /* memory pools */
    if (rc) return rc;

    rc = pgtable_init();                     /* page table ops */
    if (rc) { kmod_hook_mem_cleanup(); return rc; }

    extern void kh_write_insts_init(void);
    kh_write_insts_init();                   /* code patching */

    /* ... install hooks ... */
    return 0;
}
```

### Loading

Use `insmod` if CRC/vermagic matches, or `kmod_loader` for cross-kernel compatibility:

```bash
kmod_loader my_hook.ko    # auto-fetches kallsyms_addr from /proc/kallsyms
```

## Mode B -- SDK

Depends on `kernelhook.ko` being loaded first. Your module links against the exported symbols at runtime, resulting in a much smaller `.ko`.

### Build

```bash
cd examples/hello_hook
make -f Makefile.sdk module
```

Uses `kmod/mk/kmod_sdk.mk`. Defines `-DKH_SDK_MODE` automatically.

### Makefile Template

```makefile
MODULE_NAME  := my_hook
MODULE_SRCS  := my_hook.c
KERNELHOOK_DIR := /path/to/KernelHook/kmod
include $(KERNELHOOK_DIR)/mk/kmod_sdk.mk
```

### Source Includes

```c
#include <kernelhook/hook.h>
#include <kernelhook/types.h>
```

### Init Sequence

No subsystem init needed -- `kernelhook.ko` handles it:

```c
static int __init my_hook_init(void)
{
    void *target = (void *)ksyms_lookup("do_sys_openat2");
    hook_err_t err = hook_wrap4(target, my_before, my_after, NULL);
    /* ... */
    return 0;
}
```

### Loading

Load `kernelhook.ko` first, then your module:

```bash
insmod kernelhook.ko kallsyms_addr=0x...
insmod my_hook.ko
```

## Mode C -- Kbuild

Standard Linux kernel module build. Requires kernel source tree or pre-built headers.

### Build

```bash
make -C /path/to/kernel M=$(pwd) modules
```

### Kbuild Template

```makefile
# Kbuild
KH_ROOT := /path/to/KernelHook

obj-m := my_hook.o
my_hook-y := my_hook_main.o \
    $(KH_ROOT)/src/hook.o \
    $(KH_ROOT)/src/hmem.o \
    $(KH_ROOT)/src/ksyms.o \
    $(KH_ROOT)/src/arch/arm64/inline.o \
    $(KH_ROOT)/src/arch/arm64/transit.o \
    $(KH_ROOT)/src/arch/arm64/insn.o \
    $(KH_ROOT)/src/arch/arm64/pgtable.o

ccflags-y := -I$(KH_ROOT)/include -I$(KH_ROOT)/include/arch/arm64
```

### Loading

Standard `insmod` -- the module is built against the exact kernel, so CRC/vermagic matches:

```bash
insmod my_hook.ko kallsyms_addr=0x...
```
