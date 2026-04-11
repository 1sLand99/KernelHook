/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Fake <linux/version.h> for freestanding .ko builds.
 *
 * Provides KERNEL_VERSION() macro for compile-time version checks.
 * In freestanding mode, runtime version detection is done via
 * linux_banner parsing (see compat.c), so LINUX_VERSION_CODE is
 * not meaningful — set to 0.
 */

#ifndef _FAKE_LINUX_VERSION_H
#define _FAKE_LINUX_VERSION_H

#define KERNEL_VERSION(a, b, c) (((a) << 16) + ((b) << 8) + (c))

#ifndef LINUX_VERSION_CODE
#define LINUX_VERSION_CODE 0
#endif

#endif /* _FAKE_LINUX_VERSION_H */
