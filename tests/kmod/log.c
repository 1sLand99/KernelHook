/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2026 bmax121.
 *
 * Test module log backend: provides printk/log_level for the kmod test
 * harness across freestanding (ksyms-resolved) and kbuild (direct) paths.
 *
 * Build modes: kernel
 * Depends on: <linux/printk.h> (kbuild); ksyms_lookup("_printk") (freestanding)
 * Notes: NOT a copy of src/platform/log.c — test-only adaptation.
 *   SDK mode (KH_SDK_MODE) guards this file out entirely; log_level comes
 *   from the shim's <linux/printk.h> as a TU-local static in that case.
 */

#ifndef KH_SDK_MODE

#include <linux/printk.h>
#if __has_include(<linux/stdarg.h>)
#include <linux/stdarg.h>
#else
#include <stdarg.h>
#endif
#include <symbol.h>

/* kh_hook.h provides KCFI_EXEMPT */
#include <kh_hook.h>

/* LOG_INFO is defined by the freestanding shim printk.h but not by the
 * real kernel <linux/printk.h>.  Define it here for kbuild builds. */
#ifndef LOG_INFO
#define LOG_INFO  6
#define LOG_ERR   3
#define LOG_WARN  4
#define LOG_DEBUG 7
#endif

int log_level = LOG_INFO;

#ifdef KMOD_FREESTANDING

/* vprintk for KCFI-safe variadic forwarding */
typedef int (*vprintk_func_t)(const char *fmt, va_list args);
static vprintk_func_t kh_vprintk_func = NULL;

KCFI_EXEMPT
int printk(const char *fmt, ...)
{
    if (!kh_vprintk_func) return 0;
    va_list args;
    va_start(args, fmt);
    int ret = kh_vprintk_func(fmt, args);
    va_end(args);
    return ret;
}

int log_init(void)
{
    kh_vprintk_func = (vprintk_func_t)(uintptr_t)ksyms_lookup("vprintk");
    if (!kh_vprintk_func)
        return -1;
    return 0;
}

#else /* Kbuild */

#include <linux/moduleparam.h>
module_param(log_level, int, 0644);
MODULE_PARM_DESC(log_level, "Runtime log level (3=err 4=warn 6=info 7=debug)");

int log_init(void)
{
    return 0;
}

#endif /* KMOD_FREESTANDING */

#endif /* !KH_SDK_MODE */
