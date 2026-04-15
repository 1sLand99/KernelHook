/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2026 bmax121.
 *
 * Kernel symbol resolver implementation: stores the kallsyms_lookup_name
 * function pointer and dispatches ksyms_lookup() calls through it.
 *
 * Build modes: shared
 * Depends on: symbol.h, types.h, kh_hook.h
 */

#include <types.h>
#include <symbol.h>
#include <kh_hook.h>

typedef uint64_t (*kallsyms_lookup_name_func_t)(const char *name);

static kallsyms_lookup_name_func_t kallsyms_lookup_name_fn = NULL;

int ksyms_init(uint64_t kallsyms_lookup_name_addr)
{
    if (!kallsyms_lookup_name_addr)
        return -1;
    kallsyms_lookup_name_fn = (kallsyms_lookup_name_func_t)kallsyms_lookup_name_addr;
    return 0;
}

KCFI_EXEMPT
uint64_t ksyms_lookup(const char *name)
{
    if (!kallsyms_lookup_name_fn || !name)
        return 0;
    return kallsyms_lookup_name_fn(name);
}
