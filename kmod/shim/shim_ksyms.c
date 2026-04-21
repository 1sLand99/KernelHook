/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Freestanding shim wrappers for genuinely-kernel functions.
 *
 * Mode C (kbuild) gets these functions directly from the real kernel
 * headers + kernel symbol table; this file is a no-op in that mode
 * (guarded by KMOD_FREESTANDING).
 *
 * Pattern: each wrapper looks up its target kernel function once via
 * ksyms_lookup(), caches the address in a static function-pointer slot,
 * and thereafter calls through the cached pointer. This is the rule
 * documented in memory/feedback_ksyms_over_extern.md — it avoids
 * compile-time extern references that would otherwise require
 * MODVERSIONS CRC resolution at module load.
 *
 * Safety invariant: every wrapper must behave sensibly BEFORE the
 * first successful ksyms_lookup (i.e. before kmod_compat_init /
 * ksyms_init has patched kallsyms_addr). Acceptable early-call
 * behavior is "silent no-op that returns a non-success value" — the
 * caller sees the same outcome it would see if the kernel function
 * had failed. This mirrors log.c's printk wrapper, which silently
 * returns 0 before log_init() runs.
 *
 * KCFI note: indirect calls through ksyms-resolved function pointers
 * trip kCFI's type-hash check because the kernel function's hash was
 * generated against the kernel's CFI shadow, not our module's. Every
 * wrapper here therefore carries __attribute__((no_sanitize("kcfi")))
 * (aliased to KCFI_EXEMPT via kh_hook.h).
 */

#include <types.h>

#ifndef KMOD_FREESTANDING
/* Mode C (kbuild) path: kernel provides these symbols directly.
 * This compilation unit emits no symbols. */
#else

#include <linux/types.h>   /* umode_t, etc. */
#include <linux/kernel.h>  /* add_taint, kstrtol declarations */
#include <linux/uaccess.h> /* copy_to/from_user declarations */
#include <linux/string.h>  /* snprintf declaration */
#include <symbol.h>        /* ksyms_lookup */
#include <kh_hook.h>       /* KCFI_EXEMPT */

#if __has_include(<linux/stdarg.h>)
#include <linux/stdarg.h>
#else
#include <stdarg.h>
#endif

/* ========================================================================
 * add_taint(unsigned flag, int lockdep_ok) -> void
 * ======================================================================== */

typedef void (*add_taint_fn_t)(unsigned, int);
static add_taint_fn_t kh_add_taint_fn;

KCFI_EXEMPT
void add_taint(unsigned flag, int lockdep_ok)
{
    if (!kh_add_taint_fn)
        kh_add_taint_fn = (add_taint_fn_t)(uintptr_t)ksyms_lookup("add_taint");
    if (!kh_add_taint_fn) return;   /* silent no-op */
    kh_add_taint_fn(flag, lockdep_ok);
}

/* ========================================================================
 * copy_from_user / copy_to_user -> unsigned long (bytes NOT copied)
 *
 * Kernel returns 0 on success, > 0 on partial failure. We return `n`
 * (nothing copied) if the symbol isn't resolved yet, mimicking a
 * full failure — the caller sees no data transferred and handles the
 * error normally.
 *
 * NOTE on symbol naming: these wrappers are prefixed `kh_shim_*` (not
 * `copy_to_user` / `copy_from_user`) so they don't collide with the
 * kernel-side names in kallsyms. If we exported `copy_to_user` as a
 * global, `ksyms_lookup("copy_to_user")` from the strategy resolver
 * (src/strategies/uaccess_copy.c prio 1) would find our own module
 * symbol first on kernels that don't export the bare names — on Pixel 6
 * GKI 6.1, for example, only `__arch_copy_to_user` is in kallsyms, not
 * `copy_to_user` / `_copy_to_user`. Finding our own wrapper produced a
 * self-call recursion that stack-overflowed the kernel.
 *
 * Callers keep using `copy_to_user(...)` — the shim header aliases it
 * to `kh_shim_copy_to_user` via a static-inline wrapper.
 * ======================================================================== */

typedef unsigned long (*copy_from_user_fn_t)(void *, const void __user *, unsigned long);
typedef unsigned long (*copy_to_user_fn_t)(void __user *, const void *, unsigned long);

static copy_from_user_fn_t kh_copy_from_user_fn;
static copy_to_user_fn_t   kh_copy_to_user_fn;

KCFI_EXEMPT
unsigned long kh_shim_copy_from_user(void *to, const void __user *from, unsigned long n)
{
    if (!kh_copy_from_user_fn)
        kh_copy_from_user_fn = (copy_from_user_fn_t)(uintptr_t)ksyms_lookup("_copy_from_user");
    if (!kh_copy_from_user_fn)
        kh_copy_from_user_fn = (copy_from_user_fn_t)(uintptr_t)ksyms_lookup("copy_from_user");
    if (!kh_copy_from_user_fn)
        kh_copy_from_user_fn = (copy_from_user_fn_t)(uintptr_t)ksyms_lookup("__arch_copy_from_user");
    if (!kh_copy_from_user_fn)
        kh_copy_from_user_fn = (copy_from_user_fn_t)(uintptr_t)ksyms_lookup("__copy_from_user");
    if (!kh_copy_from_user_fn) return n;
    return kh_copy_from_user_fn(to, from, n);
}

KCFI_EXEMPT
unsigned long kh_shim_copy_to_user(void __user *to, const void *from, unsigned long n)
{
    if (!kh_copy_to_user_fn)
        kh_copy_to_user_fn = (copy_to_user_fn_t)(uintptr_t)ksyms_lookup("_copy_to_user");
    if (!kh_copy_to_user_fn)
        kh_copy_to_user_fn = (copy_to_user_fn_t)(uintptr_t)ksyms_lookup("copy_to_user");
    if (!kh_copy_to_user_fn)
        kh_copy_to_user_fn = (copy_to_user_fn_t)(uintptr_t)ksyms_lookup("__arch_copy_to_user");
    if (!kh_copy_to_user_fn)
        kh_copy_to_user_fn = (copy_to_user_fn_t)(uintptr_t)ksyms_lookup("__copy_to_user");
    if (!kh_copy_to_user_fn) return n;
    return kh_copy_to_user_fn(to, from, n);
}

/* ========================================================================
 * kstrtol(const char *, unsigned int base, long *res) -> int
 *
 * Returns 0 on success, -errno otherwise. If not resolved, return
 * -ENOSYS (-38) and leave *res untouched.
 * ======================================================================== */

typedef int (*kstrtol_fn_t)(const char *, unsigned int, long *);
static kstrtol_fn_t kh_kstrtol_fn;

KCFI_EXEMPT
int kstrtol(const char *s, unsigned int base, long *res)
{
    if (!kh_kstrtol_fn)
        kh_kstrtol_fn = (kstrtol_fn_t)(uintptr_t)ksyms_lookup("kstrtol");
    if (!kh_kstrtol_fn) return -38;   /* -ENOSYS */
    return kh_kstrtol_fn(s, base, res);
}

/* ========================================================================
 * vsnprintf / snprintf
 *
 * snprintf is implemented locally (pure varargs forwarding) so it doesn't
 * need a separate ksyms wrapper. vsnprintf does the real work via ksyms.
 *
 * When vsnprintf isn't resolved yet, wrapper writes "" to the buffer (if
 * size > 0) and returns 0. This is safer than leaving uninitialized
 * memory, though callers should typically check the return value anyway.
 * ======================================================================== */

typedef int (*vsnprintf_fn_t)(char *, unsigned long, const char *, va_list);
static vsnprintf_fn_t kh_vsnprintf_fn;

KCFI_EXEMPT
int vsnprintf(char *buf, unsigned long size, const char *fmt, va_list args)
{
    if (!kh_vsnprintf_fn)
        kh_vsnprintf_fn = (vsnprintf_fn_t)(uintptr_t)ksyms_lookup("vsnprintf");
    if (!kh_vsnprintf_fn) {
        if (size) buf[0] = '\0';
        return 0;
    }
    return kh_vsnprintf_fn(buf, size, fmt, args);
}

int snprintf(char *buf, unsigned long size, const char *fmt, ...)
{
    va_list ap;
    int r;
    va_start(ap, fmt);
    r = vsnprintf(buf, size, fmt, ap);
    va_end(ap);
    return r;
}

#endif /* KMOD_FREESTANDING */
