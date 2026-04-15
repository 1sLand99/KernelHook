/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2026 bmax121.
 *
 * Hook memory pool API: kh_mem_ops_t callbacks and bitmap allocator
 * interface for ROX (execute) and RW (data) pools.
 *
 * Build modes: shared
 * Depends on: types.h
 */

#ifndef _KP_MEMORY_H_
#define _KP_MEMORY_H_

#include <types.h>

/* External allocator/permission callbacks.
 * alloc must return page-aligned memory. */
typedef struct {
    void *(*alloc)(uint64_t size);
    void (*free)(void *ptr);
    int (*set_memory_rw)(uintptr_t addr, int numpages);
    int (*set_memory_ro)(uintptr_t addr, int numpages);
    int (*set_memory_x)(uintptr_t addr, int numpages);
} kh_mem_ops_t;

int kh_mem_init(const kh_mem_ops_t *rox_ops, const kh_mem_ops_t *rw_ops, uintptr_t page_sz);
void kh_mem_cleanup(void);

void *kh_mem_alloc_rox(size_t size);
void *kh_mem_alloc_rw(size_t size);

void kh_mem_free_rox(void *ptr, size_t size);
void kh_mem_free_rw(void *ptr, size_t size);

int kh_mem_rox_write_enable(void *ptr, size_t size);
int kh_mem_rox_write_disable(void *ptr, size_t size);

int kh_mem_register_origin(uintptr_t origin_addr, void *rox_ptr);
void kh_mem_unregister_origin(uintptr_t origin_addr);
void *kh_mem_get_rox_from_origin(uintptr_t origin_addr);
void *kh_mem_get_rw_from_origin(uintptr_t origin_addr);

/* Test/debug: query pool utilisation (used blocks count). */
uint32_t kh_mem_rox_used_blocks(void);
uint32_t kh_mem_rw_used_blocks(void);

/* ROX pool base address and size (for cleanup — must set_memory_rw before vfree). */
uintptr_t kh_mem_rox_pool_base(void);
uintptr_t kh_mem_rox_pool_size(void);

#endif /* _KP_MEMORY_H_ */
