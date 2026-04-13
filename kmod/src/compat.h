/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef _KMOD_COMPAT_H_
#define _KMOD_COMPAT_H_

#include <types.h>

extern int kmod_kernel_major;
extern int kmod_kernel_minor;
extern int kmod_kernel_patch;

int kmod_compat_init(unsigned long kallsyms_addr);

#endif
