/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Fake <linux/printk.h> for freestanding .ko builds.
 *
 * Kernel 6.1+ exports _printk; older kernels export printk.
 * We extern _printk and alias printk to it.
 */

#ifndef _FAKE_LINUX_PRINTK_H
#define _FAKE_LINUX_PRINTK_H

extern int _printk(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
#define printk _printk

#define KERN_INFO    "\001" "6"
#define KERN_ERR     "\001" "3"
#define KERN_WARNING "\001" "4"

#define pr_info(fmt, ...)  _printk(KERN_INFO fmt, ##__VA_ARGS__)
#define pr_err(fmt, ...)   _printk(KERN_ERR fmt, ##__VA_ARGS__)
#define pr_warn(fmt, ...)  _printk(KERN_WARNING fmt, ##__VA_ARGS__)

#endif /* _FAKE_LINUX_PRINTK_H */
