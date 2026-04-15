/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2026 bmax121.
 *
 * 64-bit ARM64 syscall number-to-name table, indexed by syscall number;
 * used by syscall.c to resolve __arm64_sys_<name> via kallsyms.
 *
 * Build modes: shared
 * Depends on: types.h
 * Notes: Ported from KernelPatch kernel/patch/common/sysname.c, compat
 *   table dropped. Names carry sys_ prefix; syscall.c prepends __arm64_
 *   and probes .cfi/.cfi_jt suffixes for CFI-jump-table builds.
 */

#ifndef _KH_SYSCALL_NAMES_H_
#define _KH_SYSCALL_NAMES_H_

#include <types.h>

/* Table capacity — matches KernelPatch (covers __NR_cachestat = 451). */
#define KH_SYSCALL_NAME_TABLE_SIZE 460

struct kh_syscall_name_entry {
    const char *name;   /* "sys_openat" etc.; NULL for reserved slots */
    uintptr_t   addr;   /* lazy-filled by kh_syscalln_name_addr() */
};

extern struct kh_syscall_name_entry
    kh_syscall_name_table[KH_SYSCALL_NAME_TABLE_SIZE];

#endif /* _KH_SYSCALL_NAMES_H_ */
