/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Fake <linux/mm.h> for freestanding .ko builds.
 * Provides PAGE_SIZE.
 */

#ifndef _FAKE_LINUX_MM_H
#define _FAKE_LINUX_MM_H

#ifndef PAGE_SIZE
#define PAGE_SIZE 4096UL
#endif

#endif /* _FAKE_LINUX_MM_H */
