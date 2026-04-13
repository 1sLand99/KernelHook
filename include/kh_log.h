/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Unified logging header for all build modes.
 *
 * - Kbuild:       real <linux/printk.h>
 * - Freestanding:  shim <linux/printk.h> (ksyms-resolved printk)
 * - Userspace:     printk implemented via vprintf in platform/log.c
 */

#ifndef _KH_LOG_H_
#define _KH_LOG_H_

#ifdef __USERSPACE__

#define LOG_ERR     3
#define LOG_WARN    4
#define LOG_INFO    6
#define LOG_DEBUG   7

extern int log_level;

int printk(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

#define pr_err(fmt, ...)   do { if (LOG_ERR   <= log_level) \
    printk("[KH/E] " fmt "\n", ##__VA_ARGS__); } while (0)
#define pr_warn(fmt, ...)  do { if (LOG_WARN  <= log_level) \
    printk("[KH/W] " fmt "\n", ##__VA_ARGS__); } while (0)
#define pr_info(fmt, ...)  do { if (LOG_INFO  <= log_level) \
    printk("[KH/I] " fmt "\n", ##__VA_ARGS__); } while (0)
#define pr_debug(fmt, ...) do { if (LOG_DEBUG <= log_level) \
    printk("[KH/D] " fmt "\n", ##__VA_ARGS__); } while (0)

#else /* Kernel (freestanding or kbuild) */

#include <linux/printk.h>

#endif /* __USERSPACE__ */

#endif /* _KH_LOG_H_ */
