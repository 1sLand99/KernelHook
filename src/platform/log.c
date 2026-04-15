/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2026 bmax121.
 *
 * Userspace log backend: provides the printk() symbol used by kh_log.h
 * macros, implemented as a thin wrapper around vprintf.
 *
 * Build modes: shared
 * Depends on: <stdio.h>, <stdarg.h>
 */

#include <stdio.h>
#include <stdarg.h>

int log_level = 6; /* LOG_INFO */

int printk(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int ret = vprintf(fmt, args);
    va_end(args);
    return ret;
}
