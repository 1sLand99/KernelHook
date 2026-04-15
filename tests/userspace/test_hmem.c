/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Unit tests for the bitmap allocator (memory.c).
 */

#include "test_framework.h"
#include <hmem_user.h>
#include <memory.h>
#include <kh_hook.h>

/* Block size matches memory.c internal BLOCK_SIZE */
#define BLOCK_SIZE 64

/* ---- Setup/teardown helpers ---- */

static void hmem_setup(void)
{
    int rc = kh_hmem_user_init();
    ASSERT_EQ(rc, 0);
}

static void hmem_teardown(void)
{
    kh_hmem_user_cleanup();
}

/* ---- Tests ---- */

TEST(hmem_alloc_rox_single)
{
    hmem_setup();
    void *p = kh_mem_alloc_rox(BLOCK_SIZE);
    ASSERT_NOT_NULL(p);
    kh_mem_free_rox(p, BLOCK_SIZE);
    hmem_teardown();
}

TEST(hmem_alloc_rw_single)
{
    hmem_setup();
    void *p = kh_mem_alloc_rw(BLOCK_SIZE);
    ASSERT_NOT_NULL(p);
    kh_mem_free_rw(p, BLOCK_SIZE);
    hmem_teardown();
}

TEST(hmem_alloc_rox_exhaust)
{
    hmem_setup();

    /* ROX pool is 1MB, block size 64 => 16384 blocks.
     * Allocate until exhausted. */
    #define ROX_POOL_SIZE (1024 * 1024)
    #define MAX_ALLOCS (ROX_POOL_SIZE / BLOCK_SIZE)

    /* Allocate one large chunk that fills the pool */
    void *big = kh_mem_alloc_rox(ROX_POOL_SIZE);
    ASSERT_NOT_NULL(big);

    /* Next allocation should fail */
    void *overflow = kh_mem_alloc_rox(BLOCK_SIZE);
    ASSERT_NULL(overflow);

    kh_mem_free_rox(big, ROX_POOL_SIZE);
    hmem_teardown();
}

TEST(hmem_free_and_reuse)
{
    hmem_setup();

    void *p1 = kh_mem_alloc_rox(BLOCK_SIZE);
    ASSERT_NOT_NULL(p1);

    kh_mem_free_rox(p1, BLOCK_SIZE);

    /* After free, re-allocating same size should return same address
     * (first-fit in bitmap allocator). */
    void *p2 = kh_mem_alloc_rox(BLOCK_SIZE);
    ASSERT_EQ((uintptr_t)p1, (uintptr_t)p2);

    kh_mem_free_rox(p2, BLOCK_SIZE);
    hmem_teardown();
}

TEST(hmem_origin_register_lookup)
{
    hmem_setup();

    void *rox = kh_mem_alloc_rox(sizeof(uint64_t) * 8);
    ASSERT_NOT_NULL(rox);

    uint64_t origin = 0xDEADBEEF;

    kh_mem_register_origin(origin, rox);

    /* Lookup ROX from origin */
    void *found_rox = kh_mem_get_rox_from_origin(origin);
    ASSERT_EQ((uintptr_t)found_rox, (uintptr_t)rox);

    /* Unregister and verify it's gone */
    kh_mem_unregister_origin(origin);
    void *gone = kh_mem_get_rox_from_origin(origin);
    ASSERT_NULL(gone);

    kh_mem_free_rox(rox, sizeof(uint64_t) * 8);
    hmem_teardown();
}

TEST(hmem_origin_rw_lookup)
{
    hmem_setup();

    /* Allocate ROX + RW and link them (simulating kh_hook_wrap) */
    kh_hook_chain_rox_t *rox =
        (kh_hook_chain_rox_t *)kh_mem_alloc_rox(sizeof(kh_hook_chain_rox_t));
    ASSERT_NOT_NULL(rox);

    kh_hook_chain_rw_t *rw =
        (kh_hook_chain_rw_t *)kh_mem_alloc_rw(sizeof(kh_hook_chain_rw_t));
    ASSERT_NOT_NULL(rw);

    /* Link rox->rw (requires write-enabling ROX memory) */
    kh_mem_rox_write_enable(rox, sizeof(kh_hook_chain_rox_t));
    rox->rw = rw;
    kh_mem_rox_write_disable(rox, sizeof(kh_hook_chain_rox_t));

    uint64_t origin = 0xCAFEBABE;
    kh_mem_register_origin(origin, rox);

    /* Lookup RW via origin */
    void *found_rw = kh_mem_get_rw_from_origin(origin);
    ASSERT_EQ((uintptr_t)found_rw, (uintptr_t)rw);

    kh_mem_unregister_origin(origin);
    kh_mem_free_rw(rw, sizeof(kh_hook_chain_rw_t));
    kh_mem_free_rox(rox, sizeof(kh_hook_chain_rox_t));
    hmem_teardown();
}

TEST(hmem_rox_write_enable_disable)
{
    hmem_setup();

    void *p = kh_mem_alloc_rox(BLOCK_SIZE);
    ASSERT_NOT_NULL(p);

    /* Enable write on ROX page */
    int rc = kh_mem_rox_write_enable(p, BLOCK_SIZE);
    ASSERT_EQ(rc, 0);

    /* Write should succeed (no crash = pass) */
    memset(p, 0xAA, BLOCK_SIZE);

    /* Disable write (restore ROX) */
    rc = kh_mem_rox_write_disable(p, BLOCK_SIZE);
    ASSERT_EQ(rc, 0);

    kh_mem_free_rox(p, BLOCK_SIZE);
    hmem_teardown();
}

TEST(hmem_alloc_rw_exhaust)
{
    hmem_setup();

    /* RW pool is 512KB */
    #define RW_POOL_SIZE (512 * 1024)

    void *big = kh_mem_alloc_rw(RW_POOL_SIZE);
    ASSERT_NOT_NULL(big);

    void *overflow = kh_mem_alloc_rw(BLOCK_SIZE);
    ASSERT_NULL(overflow);

    kh_mem_free_rw(big, RW_POOL_SIZE);
    hmem_teardown();
}

int main(void)
{
    return RUN_ALL_TESTS();
}
