/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2026 bmax121.
 *
 * Kernel symbol resolver: ksyms_init bootstraps kallsyms_lookup_name,
 * ksyms_lookup resolves symbol addresses at runtime.
 *
 * Build modes: shared
 * Depends on: types.h; kallsyms_lookup_name address passed at module init
 */

#ifndef _KP_SYMBOL_H_
#define _KP_SYMBOL_H_

#include <types.h>

int ksyms_init(uint64_t kallsyms_lookup_name_addr);
uint64_t ksyms_lookup(const char *name);

#endif /* _KP_SYMBOL_H_ */
