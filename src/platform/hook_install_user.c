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
#define memcpy __builtin_memcpy

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

extern uint64_t _transit(void);
extern void _transit_end(void);
extern uint64_t _fp_transit(void);
extern void _fp_transit_end(void);

static uint64_t stub_size(void *start, void *end)
{
    if (end)
        return (uintptr_t)end - (uintptr_t)start;
    return (TRANSIT_INST_NUM - 2) * sizeof(uint32_t);
}

/*
 * Generic transit buffer setup.
 *
 * Layout:
 *   transit[0..1] = uint64_t self-pointer to the containing rox struct
 *   transit[2..]  = copied asm stub machine code
 *
 * The ROX memory must be writable before calling this function.
 */
static void setup_transit(void *rox, uint32_t *transit,
                           void *stub_start, void *stub_end)
{
    *(uint64_t *)&transit[0] = (uint64_t)rox;
    uint64_t sz = stub_size(stub_start, stub_end);
    memcpy(&transit[2], stub_start, sz);
}

void hook_chain_setup_transit(hook_chain_rox_t *rox)
{
    setup_transit(rox, rox->transit,
                  (void *)(uintptr_t)_transit, (void *)(uintptr_t)_transit_end);
}

void fp_hook_chain_setup_transit(fp_hook_chain_rox_t *rox)
{
    setup_transit(rox, rox->transit,
                  (void *)(uintptr_t)_fp_transit, (void *)(uintptr_t)_fp_transit_end);
}
