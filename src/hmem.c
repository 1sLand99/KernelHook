/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2023 bmax121. All Rights Reserved.
 * Memory management: bitmap allocator with separate ROX and RW pools.
 */

#include <ktypes.h>
#include <hmem.h>
#include <ksyms.h>
#include <hook.h>
#include <log.h>
#include <export.h>

/* Pool configuration */
#define ROX_POOL_SIZE       (1024 * 1024)   /* 1MB */
#define RW_POOL_SIZE        (512 * 1024)    /* 512KB */
#define BLOCK_SIZE          64              /* 64 bytes per block */

#define ROX_TOTAL_BLOCKS    (ROX_POOL_SIZE / BLOCK_SIZE)
#define RW_TOTAL_BLOCKS     (RW_POOL_SIZE / BLOCK_SIZE)

#define ROX_BITMAP_SIZE     ((ROX_TOTAL_BLOCKS + 7) / 8)
#define RW_BITMAP_SIZE      ((RW_TOTAL_BLOCKS + 7) / 8)

/* Page size constant (4K default, ARM64 runtime may differ) */
#define HMEM_PAGE_SIZE      4096

/* ---- bitmap_pool_t ---- */

typedef struct {
    uint64_t pool_base;
    uint64_t pool_size;
    uint8_t *bitmap;
    uint32_t total_blocks;
    uint32_t used_blocks;
    uint32_t block_size;
} bitmap_pool_t;

/* Static pools and their bitmaps */
static bitmap_pool_t g_rox_pool;
static bitmap_pool_t g_rw_pool;

static uint8_t rox_bitmap[ROX_BITMAP_SIZE];
static uint8_t rw_bitmap[RW_BITMAP_SIZE];

/* Kernel function pointers resolved via ksyms */
typedef void *(*module_alloc_func_t)(uint64_t size);
typedef void (*module_memfree_func_t)(void *ptr);
typedef void *(*vmalloc_func_t)(uint64_t size);
typedef void (*vfree_func_t)(const void *addr);
typedef int (*set_memory_rw_func_t)(uint64_t addr, int numpages);
typedef int (*set_memory_ro_func_t)(uint64_t addr, int numpages);
typedef int (*set_memory_x_func_t)(uint64_t addr, int numpages);

static module_alloc_func_t kfn_module_alloc;
static module_memfree_func_t kfn_module_memfree;
static vmalloc_func_t kfn_vmalloc;
static vfree_func_t kfn_vfree;
static set_memory_rw_func_t kfn_set_memory_rw;
static set_memory_ro_func_t kfn_set_memory_ro;
static set_memory_x_func_t kfn_set_memory_x;

/* ---- Local memset (no libc) ---- */

static void kp_memset(void *dst, int val, size_t n)
{
    uint8_t *p = (uint8_t *)dst;
    while (n--)
        *p++ = (uint8_t)val;
}

/* ---- Bit operations ---- */

static inline void bitmap_set(uint8_t *bm, uint32_t bit)
{
    bm[bit / 8] |= (uint8_t)(1 << (bit % 8));
}

static inline void bitmap_clear(uint8_t *bm, uint32_t bit)
{
    bm[bit / 8] &= (uint8_t)~(1 << (bit % 8));
}

static inline int bitmap_test(const uint8_t *bm, uint32_t bit)
{
    return (bm[bit / 8] >> (bit % 8)) & 1;
}

/* ---- Bitmap allocator core ---- */

static int bitmap_find_free(bitmap_pool_t *pool, uint32_t blocks_needed)
{
    uint32_t consecutive = 0;
    int start = -1;

    for (uint32_t i = 0; i < pool->total_blocks; i++) {
        if (!bitmap_test(pool->bitmap, i)) {
            if (consecutive == 0)
                start = (int)i;
            consecutive++;
            if (consecutive >= blocks_needed)
                return start;
        } else {
            consecutive = 0;
            start = -1;
        }
    }
    return -1;
}

static void *bitmap_alloc(bitmap_pool_t *pool, size_t size)
{
    if (!pool->pool_base || size == 0)
        return NULL;

    uint32_t blocks_needed = (uint32_t)((size + pool->block_size - 1) / pool->block_size);
    int start = bitmap_find_free(pool, blocks_needed);
    if (start < 0)
        return NULL;

    for (uint32_t i = 0; i < blocks_needed; i++)
        bitmap_set(pool->bitmap, (uint32_t)start + i);

    pool->used_blocks += blocks_needed;

    void *ptr = (void *)(pool->pool_base + (uint64_t)start * pool->block_size);
    kp_memset(ptr, 0, (size_t)blocks_needed * pool->block_size);
    return ptr;
}

static void bitmap_free(bitmap_pool_t *pool, void *ptr, size_t size)
{
    if (!pool->pool_base || !ptr || size == 0)
        return;

    uint64_t addr = (uint64_t)ptr;
    if (addr < pool->pool_base || addr >= pool->pool_base + pool->pool_size)
        return;

    uint32_t start_block = (uint32_t)((addr - pool->pool_base) / pool->block_size);
    uint32_t blocks = (uint32_t)((size + pool->block_size - 1) / pool->block_size);

    for (uint32_t i = 0; i < blocks; i++)
        bitmap_clear(pool->bitmap, start_block + i);

    if (pool->used_blocks >= blocks)
        pool->used_blocks -= blocks;
    else
        pool->used_blocks = 0;
}

/* ---- Pool init helpers ---- */

static int pool_init_rox(void)
{
    void *base = NULL;

    if (kfn_module_alloc)
        base = kfn_module_alloc(ROX_POOL_SIZE);

    if (!base && kfn_vmalloc)
        base = kfn_vmalloc(ROX_POOL_SIZE);

    if (!base) {
        logke("hmem: failed to allocate ROX pool");
        return -1;
    }

    kp_memset(base, 0, ROX_POOL_SIZE);
    kp_memset(rox_bitmap, 0, ROX_BITMAP_SIZE);

    g_rox_pool.pool_base = (uint64_t)base;
    g_rox_pool.pool_size = ROX_POOL_SIZE;
    g_rox_pool.bitmap = rox_bitmap;
    g_rox_pool.total_blocks = ROX_TOTAL_BLOCKS;
    g_rox_pool.used_blocks = 0;
    g_rox_pool.block_size = BLOCK_SIZE;

    logki("hmem: ROX pool at 0x%llx, size %d", (unsigned long long)g_rox_pool.pool_base, ROX_POOL_SIZE);
    return 0;
}

static int pool_init_rw(void)
{
    void *base = NULL;

    if (kfn_module_alloc)
        base = kfn_module_alloc(RW_POOL_SIZE);

    if (!base && kfn_vmalloc)
        base = kfn_vmalloc(RW_POOL_SIZE);

    if (!base) {
        logke("hmem: failed to allocate RW pool");
        return -1;
    }

    kp_memset(base, 0, RW_POOL_SIZE);
    kp_memset(rw_bitmap, 0, RW_BITMAP_SIZE);

    g_rw_pool.pool_base = (uint64_t)base;
    g_rw_pool.pool_size = RW_POOL_SIZE;
    g_rw_pool.bitmap = rw_bitmap;
    g_rw_pool.total_blocks = RW_TOTAL_BLOCKS;
    g_rw_pool.used_blocks = 0;
    g_rw_pool.block_size = BLOCK_SIZE;

    logki("hmem: RW pool at 0x%llx, size %d", (unsigned long long)g_rw_pool.pool_base, RW_POOL_SIZE);
    return 0;
}

/* ---- Public API ---- */

int hook_mem_init(void)
{
    /* Resolve kernel symbols */
    kfn_module_alloc = (module_alloc_func_t)(uintptr_t)ksyms_lookup_cache("module_alloc");
    kfn_module_memfree = (module_memfree_func_t)(uintptr_t)ksyms_lookup_cache("module_memfree");
    kfn_vmalloc = (vmalloc_func_t)(uintptr_t)ksyms_lookup_cache("vmalloc");
    kfn_vfree = (vfree_func_t)(uintptr_t)ksyms_lookup_cache("vfree");
    kfn_set_memory_rw = (set_memory_rw_func_t)(uintptr_t)ksyms_lookup_cache("set_memory_rw");
    kfn_set_memory_ro = (set_memory_ro_func_t)(uintptr_t)ksyms_lookup_cache("set_memory_ro");
    kfn_set_memory_x = (set_memory_x_func_t)(uintptr_t)ksyms_lookup_cache("set_memory_x");

    if (!kfn_module_alloc && !kfn_vmalloc) {
        logke("hmem: neither module_alloc nor vmalloc found");
        return -1;
    }

    int rc = pool_init_rox();
    if (rc)
        return rc;

    rc = pool_init_rw();
    if (rc) {
        /* Cleanup ROX pool on RW failure */
        if (kfn_module_memfree)
            kfn_module_memfree((void *)g_rox_pool.pool_base);
        else if (kfn_vfree)
            kfn_vfree((void *)g_rox_pool.pool_base);
        g_rox_pool.pool_base = 0;
        return rc;
    }

    logki("hmem: memory manager initialized");
    return 0;
}

void hook_mem_cleanup(void)
{
    if (g_rox_pool.pool_base) {
        if (kfn_module_memfree)
            kfn_module_memfree((void *)g_rox_pool.pool_base);
        else if (kfn_vfree)
            kfn_vfree((void *)g_rox_pool.pool_base);
        g_rox_pool.pool_base = 0;
    }

    if (g_rw_pool.pool_base) {
        if (kfn_module_memfree)
            kfn_module_memfree((void *)g_rw_pool.pool_base);
        else if (kfn_vfree)
            kfn_vfree((void *)g_rw_pool.pool_base);
        g_rw_pool.pool_base = 0;
    }

    logki("hmem: memory manager cleaned up");
}

void *hook_mem_alloc_rox(size_t size)
{
    return bitmap_alloc(&g_rox_pool, size);
}

void *hook_mem_alloc_rw(size_t size)
{
    return bitmap_alloc(&g_rw_pool, size);
}

void hook_mem_free_rox(void *ptr, size_t size)
{
    bitmap_free(&g_rox_pool, ptr, size);
}

void hook_mem_free_rw(void *ptr, size_t size)
{
    bitmap_free(&g_rw_pool, ptr, size);
}

int hook_mem_rox_write_enable(void *ptr, size_t size)
{
    if (!ptr || size == 0)
        return -1;

    uint64_t addr = (uint64_t)ptr;
    uint64_t page_start = addr & ~((uint64_t)HMEM_PAGE_SIZE - 1);
    uint64_t page_end = (addr + size + HMEM_PAGE_SIZE - 1) & ~((uint64_t)HMEM_PAGE_SIZE - 1);
    int numpages = (int)((page_end - page_start) / HMEM_PAGE_SIZE);

    if (kfn_set_memory_rw)
        return kfn_set_memory_rw(page_start, numpages);

    /* Fallback: page table manipulation will be handled by arch-specific pgtable module (US-006) */
    logkw("hmem: set_memory_rw not available, fallback needed");
    return -1;
}

int hook_mem_rox_write_disable(void *ptr, size_t size)
{
    if (!ptr || size == 0)
        return -1;

    uint64_t addr = (uint64_t)ptr;
    uint64_t page_start = addr & ~((uint64_t)HMEM_PAGE_SIZE - 1);
    uint64_t page_end = (addr + size + HMEM_PAGE_SIZE - 1) & ~((uint64_t)HMEM_PAGE_SIZE - 1);
    int numpages = (int)((page_end - page_start) / HMEM_PAGE_SIZE);

    int rc = 0;

    if (kfn_set_memory_ro) {
        rc = kfn_set_memory_ro(page_start, numpages);
        if (rc)
            return rc;
    }

    if (kfn_set_memory_x) {
        rc = kfn_set_memory_x(page_start, numpages);
        if (rc)
            return rc;
    }

    if (!kfn_set_memory_ro && !kfn_set_memory_x) {
        logkw("hmem: set_memory_ro/x not available, fallback needed");
        return -1;
    }

    return 0;
}

void *hook_mem_get_rox_from_origin(uint64_t origin_addr)
{
    if (!g_rox_pool.pool_base || !origin_addr)
        return NULL;

    /* Linear scan: each hook_chain_rox_t starts at an allocation boundary.
     * We scan every BLOCK_SIZE-aligned address looking for allocated blocks
     * whose hook_t.func_addr matches the origin. */
    for (uint32_t i = 0; i < g_rox_pool.total_blocks; i++) {
        if (!bitmap_test(g_rox_pool.bitmap, i))
            continue;

        void *block = (void *)(g_rox_pool.pool_base + (uint64_t)i * g_rox_pool.block_size);
        hook_chain_rox_t *rox = (hook_chain_rox_t *)block;

        if (rox->hook.func_addr == origin_addr)
            return rox;

        /* Skip remaining blocks of this allocation by looking for the next
         * free block (simple heuristic: jump over contiguous used blocks
         * that would be part of the same allocation). */
        uint32_t size_blocks = (uint32_t)((sizeof(hook_chain_rox_t) + g_rox_pool.block_size - 1) / g_rox_pool.block_size);
        if (size_blocks > 1)
            i += size_blocks - 1;
    }
    return NULL;
}

void *hook_mem_get_rw_from_origin(uint64_t origin_addr)
{
    hook_chain_rox_t *rox = (hook_chain_rox_t *)hook_mem_get_rox_from_origin(origin_addr);
    if (rox && rox->rw)
        return rox->rw;
    return NULL;
}

KP_EXPORT_SYMBOL(hook_mem_init);
KP_EXPORT_SYMBOL(hook_mem_cleanup);
KP_EXPORT_SYMBOL(hook_mem_alloc_rox);
KP_EXPORT_SYMBOL(hook_mem_alloc_rw);
KP_EXPORT_SYMBOL(hook_mem_free_rox);
KP_EXPORT_SYMBOL(hook_mem_free_rw);
KP_EXPORT_SYMBOL(hook_mem_rox_write_enable);
KP_EXPORT_SYMBOL(hook_mem_rox_write_disable);
KP_EXPORT_SYMBOL(hook_mem_get_rox_from_origin);
KP_EXPORT_SYMBOL(hook_mem_get_rw_from_origin);
