/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Fake <linux/kernel.h> for freestanding .ko builds.
 *
 * Provides pr_info / pr_err / pr_warn via _printk, matching the real
 * kernel header's public interface.
 */

#ifndef _FAKE_LINUX_KERNEL_H
#define _FAKE_LINUX_KERNEL_H

#include <linux/printk.h>

#endif /* _FAKE_LINUX_KERNEL_H */
