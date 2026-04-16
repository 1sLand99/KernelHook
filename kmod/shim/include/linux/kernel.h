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

/* add_taint — mark the kernel as tainted.
 * flag:       one of the TAINT_* constants below.
 * lockdep_ok: LOCKDEP_STILL_OK (1) if lockdep can still run after taint,
 *             LOCKDEP_NOW_UNRELIABLE (0) otherwise.
 * Using int for the second arg: it maps to enum lockdep_ok, but we avoid
 * pulling in the full enum definition for freestanding builds. */
extern void add_taint(unsigned flag, int lockdep_ok);

/* TAINT_CRAP — module is doing something unusual / insane.
 * Value 10 has been stable since Linux 3.2 (see include/linux/kernel.h). */
#ifndef TAINT_CRAP
#define TAINT_CRAP 10
#endif

/* LOCKDEP_STILL_OK — from enum lockdep_ok: lockdep remains usable after taint. */
#ifndef LOCKDEP_STILL_OK
#define LOCKDEP_STILL_OK 1
#endif

/* kstrtol — parse a signed long integer from a string.
 * Returns 0 on success and writes the result to *res.
 * base 0 = auto-detect (0x hex, 0 octal, decimal otherwise). */
extern int kstrtol(const char *s, unsigned int base, long *res);

#endif /* _FAKE_LINUX_KERNEL_H */
