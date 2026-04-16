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
#include <linux/debugfs.h> /* debugfs_* declarations */
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
 * ======================================================================== */

typedef unsigned long (*copy_from_user_fn_t)(void *, const void __user *, unsigned long);
typedef unsigned long (*copy_to_user_fn_t)(void __user *, const void *, unsigned long);

static copy_from_user_fn_t kh_copy_from_user_fn;
static copy_to_user_fn_t   kh_copy_to_user_fn;

KCFI_EXEMPT
unsigned long copy_from_user(void *to, const void __user *from, unsigned long n)
{
    if (!kh_copy_from_user_fn)
        kh_copy_from_user_fn = (copy_from_user_fn_t)(uintptr_t)ksyms_lookup("_copy_from_user");
    if (!kh_copy_from_user_fn)
        kh_copy_from_user_fn = (copy_from_user_fn_t)(uintptr_t)ksyms_lookup("copy_from_user");
    if (!kh_copy_from_user_fn) return n;
    return kh_copy_from_user_fn(to, from, n);
}

KCFI_EXEMPT
unsigned long copy_to_user(void __user *to, const void *from, unsigned long n)
{
    if (!kh_copy_to_user_fn)
        kh_copy_to_user_fn = (copy_to_user_fn_t)(uintptr_t)ksyms_lookup("_copy_to_user");
    if (!kh_copy_to_user_fn)
        kh_copy_to_user_fn = (copy_to_user_fn_t)(uintptr_t)ksyms_lookup("copy_to_user");
    if (!kh_copy_to_user_fn) return n;
    return kh_copy_to_user_fn(to, from, n);
}

/* ========================================================================
 * debugfs_create_dir / debugfs_create_file / debugfs_remove_recursive
 *
 * On kernels without CONFIG_DEBUG_FS these symbols don't exist. In that
 * case the wrapper returns NULL / is a no-op, and consumers treat the
 * NULL as "debugfs unavailable" — kh_strategy_debugfs_init() already
 * checks its return value.
 * ======================================================================== */

typedef struct dentry *(*debugfs_create_dir_fn_t)(const char *, struct dentry *);
typedef struct dentry *(*debugfs_create_file_fn_t)(const char *, unsigned short,
                                                    struct dentry *, void *,
                                                    const struct file_operations *);
typedef void (*debugfs_remove_recursive_fn_t)(struct dentry *);

static debugfs_create_dir_fn_t       kh_debugfs_create_dir_fn;
static debugfs_create_file_fn_t      kh_debugfs_create_file_fn;
static debugfs_remove_recursive_fn_t kh_debugfs_remove_recursive_fn;

KCFI_EXEMPT
struct dentry *debugfs_create_dir(const char *name, struct dentry *parent)
{
    if (!kh_debugfs_create_dir_fn)
        kh_debugfs_create_dir_fn = (debugfs_create_dir_fn_t)(uintptr_t)ksyms_lookup("debugfs_create_dir");
    if (!kh_debugfs_create_dir_fn) return (struct dentry *)0;
    return kh_debugfs_create_dir_fn(name, parent);
}

KCFI_EXEMPT
struct dentry *debugfs_create_file(const char *name, unsigned short mode,
                                    struct dentry *parent, void *data,
                                    const struct file_operations *fops)
{
    if (!kh_debugfs_create_file_fn)
        kh_debugfs_create_file_fn = (debugfs_create_file_fn_t)(uintptr_t)ksyms_lookup("debugfs_create_file");
    if (!kh_debugfs_create_file_fn) return (struct dentry *)0;
    return kh_debugfs_create_file_fn(name, mode, parent, data, fops);
}

KCFI_EXEMPT
void debugfs_remove_recursive(struct dentry *dentry)
{
    if (!kh_debugfs_remove_recursive_fn)
        kh_debugfs_remove_recursive_fn = (debugfs_remove_recursive_fn_t)(uintptr_t)ksyms_lookup("debugfs_remove_recursive");
    if (!kh_debugfs_remove_recursive_fn) return;
    kh_debugfs_remove_recursive_fn(dentry);
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
