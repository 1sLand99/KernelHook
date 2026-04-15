/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2026 bmax121.
 *
 * Unified logging header: routes pr_info/pr_err/pr_debug to the right
 * printk backend for each build mode.
 *
 * Build modes: shared
 * Depends on: <linux/printk.h> (kbuild/freestanding shim), platform/log.c (userspace)
 * Notes: Freestanding shim resolves printk via ksyms at runtime;
 *   userspace backend in src/platform/log.c wraps vprintf.
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
