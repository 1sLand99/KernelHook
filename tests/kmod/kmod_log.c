/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Kernel log backend: wire kp_log_func to printk.
 * Freestanding: resolved via ksyms at runtime.
 * Kbuild: direct printk reference.
 */

#ifdef KMOD_FREESTANDING
#include "kmod_shim.h"
#include <ksyms.h>
#else
#include <linux/kernel.h>
#endif

#include <log.h>

log_func_t kp_log_func = NULL;

int kmod_log_init(void)
{
#ifdef KMOD_FREESTANDING
    /* Kernel 6.1+ exports _printk; older kernels export printk */
    kp_log_func = (log_func_t)(uintptr_t)ksyms_lookup("_printk");
    if (!kp_log_func)
        kp_log_func = (log_func_t)(uintptr_t)ksyms_lookup("printk");
    if (!kp_log_func) return -1;
#else
    kp_log_func = (log_func_t)printk;
#endif
    return 0;
}
