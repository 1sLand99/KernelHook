/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2026 bmax121.
 *
 * Test module transit buffer setup: copies asm transit stubs into the ROX
 * buffer for kh_hook_chain_setup_transit / kh_fp_hook_chain_setup_transit.
 *
 * Build modes: kernel
 * Depends on: kh_hook.h, <linux/string.h> (memcpy)
 * Notes: NOT a copy of kmod/src/transit_setup.c — test-only adaptation.
 *   Caller holds ROX write-enable; no kh_platform_write_code() needed.
 */

#include <linux/string.h>

#include <types.h>
#include <kh_hook.h>
#include <linux/printk.h>

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
