/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Fake <linux/set_memory.h> for freestanding .ko builds.
 *
 * In freestanding mode, set_memory_rw/ro/x are resolved at runtime
 * via ksyms_lookup(). This header is intentionally empty — mem_ops.c
 * and inline.c handle resolution through typed function pointers.
 */

#ifndef _FAKE_LINUX_SET_MEMORY_H
#define _FAKE_LINUX_SET_MEMORY_H

#endif /* _FAKE_LINUX_SET_MEMORY_H */
