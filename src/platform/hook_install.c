/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2026 bmax121.
 * Userspace kh_hook_install / kh_hook_uninstall and transit buffer setup.
 *
 * Replaces kernel pgtable-based write_insts_at() with
 * kh_platform_set_rw / kh_platform_set_rx + icache flush.
 */

#include <types.h>
#include <kh_hook.h>
#include <kh_log.h>
#include <platform.h>
#define memcpy __builtin_memcpy

static void write_insts_at(uint64_t va, uint32_t *insts, int32_t count)
{
    uint64_t size = (uint64_t)count * sizeof(uint32_t);
    kh_platform_write_code(va, insts, size);
}

/* ---- Public API ---- */

void kh_hook_install(kh_hook_t *kh_hook)
{
    write_insts_at(kh_hook->origin_addr, kh_hook->tramp_insts, kh_hook->tramp_insts_num);
}

void kh_hook_uninstall(kh_hook_t *kh_hook)
{
    write_insts_at(kh_hook->origin_addr, kh_hook->origin_insts, kh_hook->tramp_insts_num);
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
    uint64_t avail = (TRANSIT_INST_NUM - 2) * sizeof(uint32_t);
    if (sz > avail) {
        pr_err("transit stub (%llu) exceeds buffer (%llu)",
              (unsigned long long)sz, (unsigned long long)avail);
        return;
    }
    memcpy(&transit[2], stub_start, sz);
}

void kh_hook_chain_setup_transit(kh_hook_chain_rox_t *rox)
{
    setup_transit(rox, rox->transit,
                  (void *)(uintptr_t)_transit, (void *)(uintptr_t)_transit_end);
}

void kh_fp_hook_chain_setup_transit(kh_fp_hook_chain_rox_t *rox)
{
    setup_transit(rox, rox->transit,
                  (void *)(uintptr_t)_fp_transit, (void *)(uintptr_t)_fp_transit_end);
}
