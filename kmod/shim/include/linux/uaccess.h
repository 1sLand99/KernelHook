/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Fake <linux/uaccess.h> for freestanding .ko builds.
 *
 * Provides copy_from_user / copy_to_user declarations and the __user
 * sparse annotation (defined as empty for freestanding compilation).
 */

#ifndef _FAKE_LINUX_UACCESS_H
#define _FAKE_LINUX_UACCESS_H

#include <linux/types.h>

/* __user is a sparse annotation marking user-space address-space pointers.
 * Sparse is not run in freestanding builds, so define it as a no-op. */
#ifndef __user
#define __user
#endif

/* loff_t: kernel file offset type (signed 64-bit).
 * ssize_t: signed size type for read/write return values.
 * Neither is provided by the freestanding <types.h> shim (which only defines
 * uint*_t / int*_t / size_t / bool); define them here where first needed. */
#ifndef _LOFF_T_DEFINED
#define _LOFF_T_DEFINED
typedef long long loff_t;
#endif

#ifndef _SSIZE_T_DEFINED
#define _SSIZE_T_DEFINED
typedef long ssize_t;
#endif

/* shim_ksyms.c exports `kh_shim_*` wrappers (not the bare kernel names) so
 * our module's ELF kallsyms cannot be picked up by ksyms_lookup("copy_to_user")
 * from the strategy resolver — see shim_ksyms.c for the full rationale. */
extern unsigned long kh_shim_copy_from_user(void *to, const void __user *from,
					    unsigned long n);
extern unsigned long kh_shim_copy_to_user(void __user *to, const void *from,
					  unsigned long n);

static inline unsigned long copy_from_user(void *to, const void __user *from,
					   unsigned long n)
{
	return kh_shim_copy_from_user(to, from, n);
}

static inline unsigned long copy_to_user(void __user *to, const void *from,
					 unsigned long n)
{
	return kh_shim_copy_to_user(to, from, n);
}

#endif /* _FAKE_LINUX_UACCESS_H */
