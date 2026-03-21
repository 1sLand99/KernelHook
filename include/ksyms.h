/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2023 bmax121. All Rights Reserved.
 */

#ifndef _KP_KSYMS_H_
#define _KP_KSYMS_H_

#include <ktypes.h>

int ksyms_init(uint64_t kallsyms_lookup_name_addr);
uint64_t ksyms_lookup(const char *name);
uint64_t ksyms_lookup_cache(const char *name);

#endif /* _KP_KSYMS_H_ */
