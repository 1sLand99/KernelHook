/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2023 bmax121. All Rights Reserved.
 */

#ifndef _KP_HMEM_H_
#define _KP_HMEM_H_

#include <ktypes.h>

int hook_mem_init(void);
void hook_mem_cleanup(void);

void *hook_mem_alloc_rox(size_t size);
void *hook_mem_alloc_rw(size_t size);

void hook_mem_free_rox(void *ptr, size_t size);
void hook_mem_free_rw(void *ptr, size_t size);

int hook_mem_rox_write_enable(void *ptr, size_t size);
int hook_mem_rox_write_disable(void *ptr, size_t size);

void *hook_mem_get_rox_from_origin(uint64_t origin_addr);
void *hook_mem_get_rw_from_origin(uint64_t origin_addr);

#endif /* _KP_HMEM_H_ */
