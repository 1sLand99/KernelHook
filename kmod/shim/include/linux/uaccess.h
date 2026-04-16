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

extern unsigned long copy_from_user(void *to, const void __user *from,
				    unsigned long n);
extern unsigned long copy_to_user(void __user *to, const void *from,
				  unsigned long n);

#endif /* _FAKE_LINUX_UACCESS_H */
