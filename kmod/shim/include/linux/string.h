/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Fake <linux/string.h> for freestanding .ko builds.
 *
 * In freestanding mode (-DKMOD_FREESTANDING), the compiler's
 * -I kmod/shim/include resolves this instead of the real kernel header.
 * In kbuild mode, this file is never on the include path.
 */

#ifndef _FAKE_LINUX_STRING_H
#define _FAKE_LINUX_STRING_H

/* The compiler may lower __builtin_memcpy/memset to real function calls,
 * so we need linkable declarations. The kernel exports these symbols. */
extern void *memset(void *s, int c, unsigned long n);
extern void *memcpy(void *dst, const void *src, unsigned long n);
extern void *memmove(void *dst, const void *src, unsigned long n);
extern int   strcmp(const char *s1, const char *s2);
extern int   strncmp(const char *s1, const char *s2, unsigned long n);

/* TODO(freestanding-integration): when kh_strategy.c is added to
 * kmod/mk/kmod.mk's _KH_CORE_SRCS (freestanding build), add
 *   _MODVER_ENTRY(__modver_strcmp,  0xDEADBExxu, "strcmp")
 *   _MODVER_ENTRY(__modver_strncmp, 0xDEADBExxu, "strncmp")
 * to MODULE_VERSIONS() in kmod/shim/shim.h.  Without these entries the
 * .ko links cleanly but fails to load on CONFIG_MODVERSIONS=y kernels with
 * "disagrees about version of symbol strcmp/strncmp". */

#endif /* _FAKE_LINUX_STRING_H */
