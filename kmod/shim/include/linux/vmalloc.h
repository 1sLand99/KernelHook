/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Fake <linux/vmalloc.h> for freestanding .ko builds.
 *
 * In freestanding mode, vmalloc/vfree are resolved at runtime via
 * ksyms_lookup(). This header provides declarations to satisfy the
 * compiler; the actual function pointers are wired up in mem_ops.c.
 */

#ifndef _FAKE_LINUX_VMALLOC_H
#define _FAKE_LINUX_VMALLOC_H

/* These are never called directly in freestanding mode — mem_ops.c
 * resolves them via ksyms and calls through typed function pointers.
 * Declarations here just keep the compiler happy for files that
 * include <linux/vmalloc.h> unconditionally. */

#endif /* _FAKE_LINUX_VMALLOC_H */
