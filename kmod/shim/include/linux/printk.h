/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Fake <linux/printk.h> for freestanding .ko builds.
 *
 * Provides pr_err / pr_warn / pr_info / pr_debug macros that resolve
 * printk at runtime via ksyms.  Compile-time CONFIG_LOG_LEVEL strips
 * calls below the threshold; runtime log_level allows further filtering.
 */

#ifndef _FAKE_LINUX_PRINTK_H
#define _FAKE_LINUX_PRINTK_H

#define LOG_ERR     3
#define LOG_WARN    4
#define LOG_INFO    6
#define LOG_DEBUG   7

#ifndef CONFIG_LOG_LEVEL
#define CONFIG_LOG_LEVEL LOG_INFO
#endif

extern int log_level;

/* Resolved to _printk / vprintk at runtime via ksyms. KCFI-safe. */
int printk(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

#define pr_err(fmt, ...)   do { if (LOG_ERR   <= CONFIG_LOG_LEVEL && \
    LOG_ERR   <= log_level) printk("[KH/E] " fmt "\n", ##__VA_ARGS__); } while (0)
#define pr_warn(fmt, ...)  do { if (LOG_WARN  <= CONFIG_LOG_LEVEL && \
    LOG_WARN  <= log_level) printk("[KH/W] " fmt "\n", ##__VA_ARGS__); } while (0)
#define pr_info(fmt, ...)  do { if (LOG_INFO  <= CONFIG_LOG_LEVEL && \
    LOG_INFO  <= log_level) printk("[KH/I] " fmt "\n", ##__VA_ARGS__); } while (0)
#define pr_debug(fmt, ...) do { if (LOG_DEBUG <= CONFIG_LOG_LEVEL && \
    LOG_DEBUG <= log_level) printk("[KH/D] " fmt "\n", ##__VA_ARGS__); } while (0)

#endif /* _FAKE_LINUX_PRINTK_H */
