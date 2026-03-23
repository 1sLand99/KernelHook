/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2026 bmax121.
 * Userspace hook_install / hook_uninstall and transit buffer setup.
 *
 * Replaces kernel pgtable-based write_insts_at() with
 * platform_set_rw / platform_set_rx + icache flush.
 */

#include <ktypes.h>
#include <hook.h>
#include <platform.h>
/* Use __builtin_memcpy for portability across freestanding and hosted. */
#define memcpy __builtin_memcpy

/*
 * Write instructions to a code page.
 * Uses platform_write_code which handles W^X safely on all platforms.
 */
static void write_insts_at(uint64_t va, uint32_t *insts, int32_t count)
{
    uint64_t size = (uint64_t)count * sizeof(uint32_t);
    platform_write_code(va, insts, size);
}

/* ---- Public API ---- */

void hook_install(hook_t *hook)
{
    write_insts_at(hook->origin_addr, hook->tramp_insts, hook->tramp_insts_num);
}

void hook_uninstall(hook_t *hook)
{
    write_insts_at(hook->origin_addr, hook->origin_insts, hook->tramp_insts_num);
}

/* ---- Transit buffer setup ---- */

/* Defined in transit.c — naked asm stub template. */
extern uint64_t _transit(void);

/* Defined in transit.c right after _transit — used to compute stub size. */
extern void _transit_end(void);

static uint64_t stub_size(void *start, void *end)
{
    if (end)
        return (uintptr_t)end - (uintptr_t)start;
    /* Conservative fallback: TRANSIT_INST_NUM uint32_t words minus the
     * 2-word (8-byte) self-pointer prefix = (TRANSIT_INST_NUM - 2) * 4.
     * In practice, the stub is much smaller than this capacity. */
    return (TRANSIT_INST_NUM - 2) * sizeof(uint32_t);
}

/*
 * Set up the transit buffer inside a hook_chain_rox_t.
 *
 * Layout:
 *   transit[0..1] = uint64_t self-pointer to the containing hook_chain_rox_t
 *   transit[2..]  = copied _transit stub machine code
 *
 * The ROX memory must be made writable before calling this function,
 * and restored to RX afterwards (caller is responsible).
 */
void hook_chain_setup_transit(hook_chain_rox_t *rox)
{
    /* Self-pointer: the transit stub uses this to locate rox in O(1). */
    *(uint64_t *)&rox->transit[0] = (uint64_t)rox;

    /* Copy the universal asm stub template. */
    uint64_t sz = stub_size((void *)(uintptr_t)_transit, (void *)(uintptr_t)_transit_end);
    memcpy(&rox->transit[2], (void *)(uintptr_t)_transit, sz);
}

/* ---- Function pointer hook transit setup ---- */

extern uint64_t _fp_transit(void);
extern void _fp_transit_end(void);

void fp_hook_chain_setup_transit(fp_hook_chain_rox_t *rox)
{
    *(uint64_t *)&rox->transit[0] = (uint64_t)rox;

    uint64_t sz = stub_size((void *)(uintptr_t)_fp_transit, (void *)(uintptr_t)_fp_transit_end);
    memcpy(&rox->transit[2], (void *)(uintptr_t)_fp_transit, sz);
}
