/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2026 bmax121.
 *
 * Hook chain management: kh_hook_chain_add/remove, kh_hook/kh_unhook,
 * kh_hook_wrap/kh_fp_hook_wrap — shared core for all build modes.
 *
 * Build modes: shared
 * Depends on: kh_hook.h (chain structs), sync.h (RCU/spinlock),
 *   memory.h (ROX/RW pool alloc), platform.h (icache flush)
 */

#include <types.h>
#include <kh_hook.h>
#include <sync.h>
#include <memory.h>
#include <platform.h>
#include <kh_log.h>

/* Flush D-cache and I-cache for a memory region that contains code.
 * Required after writing instructions to the ROX pool — the I-cache
 * may still hold stale instructions from a previous allocation at the
 * same address.
 *
 * ARM64 requires: DC CVAU (clean D-cache to PoU) → DSB ISH →
 * IC IVAU (invalidate I-cache to PoU) → DSB ISH → ISB.
 *
 * Userspace uses __builtin___clear_cache which issues the SVC for
 * CTR_EL0-based maintenance. */
static void flush_code_cache(void *addr, size_t size)
{
#if defined(__aarch64__) || defined(__arm64__)
#ifdef __USERSPACE__
    __builtin___clear_cache((char *)addr, (char *)addr + size);
#else
    uintptr_t start = (uintptr_t)addr;
    uintptr_t end = start + size;
    uintptr_t line;
    for (line = start; line < end; line += 4)
        asm volatile("dc cvau, %0" :: "r"(line) : "memory");
    asm volatile("dsb ish" ::: "memory");
    for (line = start; line < end; line += 4)
        asm volatile("ic ivau, %0" :: "r"(line) : "memory");
    asm volatile("dsb ish\n\tisb" ::: "memory");
#endif
#endif
}

/* ---- Generic chain operations (shared by inline and FP hooks) ----
 *
 * kh_hook_chain_rw_t and kh_fp_hook_chain_rw_t share the same field layout
 * for chain_items_max, items[], sorted_indices[], sorted_count.
 * Use macros to generate type-safe wrappers without code duplication.
 */

#define DEFINE_CHAIN_OPS(PREFIX, RW_TYPE, MASK_TYPE)                             \
                                                                                \
static void PREFIX##_rebuild_sorted(RW_TYPE *rw)                                \
{                                                                               \
    int32_t count = 0;                                                          \
    MASK_TYPE mask = rw->occupied_mask;                                          \
    while (mask) {                                                              \
        int32_t i = __builtin_ctz(mask);                                        \
        rw->sorted_indices[count++] = i;                                        \
        mask &= ~((MASK_TYPE)1 << i);                                           \
    }                                                                           \
    for (int32_t i = 1; i < count; i++) {                                      \
        int32_t key = rw->sorted_indices[i];                                    \
        int32_t key_pri = rw->items[key].priority;                              \
        int32_t j = i - 1;                                                      \
        while (j >= 0 && rw->items[rw->sorted_indices[j]].priority < key_pri) { \
            rw->sorted_indices[j + 1] = rw->sorted_indices[j];                 \
            j--;                                                                \
        }                                                                       \
        rw->sorted_indices[j + 1] = key;                                        \
    }                                                                           \
    rw->sorted_count = count;                                                   \
}                                                                               \
                                                                                \
static kh_hook_err_t PREFIX##_chain_add(RW_TYPE *rw, void *before, void *after,    \
                                      void *udata, int32_t priority)            \
{                                                                               \
    if (!rw) return HOOK_BAD_ADDRESS;                                           \
    kh_sync_write_lock();                                                          \
    MASK_TYPE avail = ~rw->occupied_mask;                                        \
    if (!avail) { kh_sync_write_unlock(); return HOOK_CHAIN_FULL; }               \
    int32_t slot = __builtin_ctz(avail);                                        \
    if (slot >= rw->chain_items_max) { kh_sync_write_unlock(); return HOOK_CHAIN_FULL; } \
    rw->occupied_mask |= (MASK_TYPE)1 << slot;                                  \
    kh_hook_chain_item_t *item = &rw->items[slot];                                 \
    item->priority = priority;                                                  \
    item->udata = udata;                                                        \
    item->before = before;                                                      \
    item->after = after;                                                        \
    __builtin_memset(&item->local, 0, sizeof(kh_hook_local_t));                    \
    PREFIX##_rebuild_sorted(rw);                                                \
    kh_sync_write_unlock();                                                        \
    return HOOK_NO_ERR;                                                         \
}                                                                               \
                                                                                \
static void PREFIX##_chain_remove(RW_TYPE *rw, void *before, void *after)       \
{                                                                               \
    if (!rw) return;                                                            \
    kh_sync_write_lock();                                                          \
    MASK_TYPE mask = rw->occupied_mask;                                          \
    while (mask) {                                                              \
        int32_t i = __builtin_ctz(mask);                                        \
        mask &= ~((MASK_TYPE)1 << i);                                           \
        kh_hook_chain_item_t *item = &rw->items[i];                                \
        if (item->before == before && item->after == after) {                   \
            rw->occupied_mask &= ~((MASK_TYPE)1 << i);                          \
            item->before = 0;                                                   \
            item->after = 0;                                                    \
            item->udata = 0;                                                    \
            item->priority = 0;                                                 \
            PREFIX##_rebuild_sorted(rw);                                        \
            kh_sync_write_unlock();                                                \
            return;                                                             \
        }                                                                       \
    }                                                                           \
    kh_sync_write_unlock();                                                        \
}                                                                               \
                                                                                \
static int PREFIX##_chain_all_empty(RW_TYPE *rw)                                \
{                                                                               \
    return rw->occupied_mask == 0;                                              \
}

/* Generate inline kh_hook chain ops (il_ prefix, 8 slots, uint16_t mask) */
DEFINE_CHAIN_OPS(il, kh_hook_chain_rw_t, uint16_t)

/* Generate FP kh_hook chain ops (fp_ prefix, 16 slots, uint32_t mask) */
DEFINE_CHAIN_OPS(fp, kh_fp_hook_chain_rw_t, uint32_t)

/* Public API wrappers for inline kh_hook chain */
kh_hook_err_t kh_hook_chain_add(kh_hook_chain_rw_t *rw, void *before, void *after,
                          void *udata, int32_t priority)
{
    return il_chain_add(rw, before, after, udata, priority);
}

void kh_hook_chain_remove(kh_hook_chain_rw_t *rw, void *before, void *after)
{
    il_chain_remove(rw, before, after);
}

/* ---- Simple inline kh_hook (no chain) ---- */

kh_hook_err_t kh_hook(void *func, void *replace, void **backup)
{
    if (!func || !replace || !backup)
        return HOOK_BAD_ADDRESS;

    func = STRIP_PAC(func);
    uintptr_t func_addr = (uintptr_t)func;

    if (kh_mem_get_rox_from_origin(func_addr))
        return HOOK_DUPLICATED;

    kh_hook_chain_rox_t *rox =
        (kh_hook_chain_rox_t *)kh_mem_alloc_rox(sizeof(kh_hook_chain_rox_t));
    if (!rox)
        return HOOK_NO_MEM;

    kh_mem_rox_write_enable(rox, sizeof(kh_hook_chain_rox_t));

    rox->rw = 0;

    kh_hook_t *h = &rox->kh_hook;
    h->func_addr = func_addr;
    h->origin_addr = func_addr;
    h->replace_addr = (uintptr_t)replace;
    h->relo_addr = (uintptr_t)h->relo_insts;
    h->tramp_insts_num = 0;
    h->relo_insts_num = 0;

    kh_hook_err_t err = kh_hook_prepare(h);
    if (err) {
        kh_mem_rox_write_disable(rox, sizeof(kh_hook_chain_rox_t));
        kh_mem_free_rox(rox, sizeof(kh_hook_chain_rox_t));
        return err;
    }

    kh_mem_rox_write_disable(rox, sizeof(kh_hook_chain_rox_t));
    flush_code_cache(rox, sizeof(kh_hook_chain_rox_t));

    kh_hook_install(h);

    if (kh_mem_register_origin(func_addr, rox) != 0) {
        kh_hook_uninstall(h);
        kh_mem_free_rox(rox, sizeof(kh_hook_chain_rox_t));
        return HOOK_NO_MEM;
    }

    *backup = (void *)h->relo_addr;
    return HOOK_NO_ERR;
}

void kh_unhook(void *func)
{
    if (!func)
        return;

    func = STRIP_PAC(func);
    uintptr_t func_addr = (uintptr_t)func;
    kh_hook_chain_rox_t *rox =
        (kh_hook_chain_rox_t *)kh_mem_get_rox_from_origin(func_addr);
    if (!rox)
        return;

    kh_hook_uninstall(&rox->kh_hook);
    kh_mem_unregister_origin(func_addr);
    kh_mem_free_rox(rox, sizeof(kh_hook_chain_rox_t));
}

/* ---- Chain-based inline kh_hook (kh_hook_wrap) ---- */

kh_hook_err_t kh_hook_wrap(void *func, int32_t argno, void *before,
                     void *after, void *udata, int32_t priority)
{
    if (!func)
        return HOOK_BAD_ADDRESS;

    func = STRIP_PAC(func);
    uintptr_t func_addr = (uintptr_t)func;
    kh_hook_chain_rox_t *rox;
    kh_hook_chain_rw_t *rw;

    rox = (kh_hook_chain_rox_t *)kh_mem_get_rox_from_origin(func_addr);

    if (!rox) {
        rox = (kh_hook_chain_rox_t *)kh_mem_alloc_rox(sizeof(kh_hook_chain_rox_t));
        if (!rox)
            return HOOK_NO_MEM;

        rw = (kh_hook_chain_rw_t *)kh_mem_alloc_rw(sizeof(kh_hook_chain_rw_t));
        if (!rw) {
            kh_mem_free_rox(rox, sizeof(kh_hook_chain_rox_t));
            return HOOK_NO_MEM;
        }

        __builtin_memset(rw, 0, sizeof(kh_hook_chain_rw_t));
        rw->rox = rox;
        rw->chain_items_max = HOOK_CHAIN_NUM;
        rw->argno = argno;
        rw->sorted_count = 0;

        kh_mem_rox_write_enable(rox, sizeof(kh_hook_chain_rox_t));

        rox->rw = rw;

        kh_hook_t *h = &rox->kh_hook;
        h->func_addr = func_addr;
        h->origin_addr = func_addr;
        h->replace_addr = (uintptr_t)&rox->transit[2]; /* transit stub entry */
        h->relo_addr = (uintptr_t)h->relo_insts;
        h->tramp_insts_num = 0;
        h->relo_insts_num = 0;

        kh_hook_err_t err = kh_hook_prepare(h);
        if (err) {
            kh_mem_rox_write_disable(rox, sizeof(kh_hook_chain_rox_t));
            kh_mem_free_rox(rox, sizeof(kh_hook_chain_rox_t));
            kh_mem_free_rw(rw, sizeof(kh_hook_chain_rw_t));
            return err;
        }

        kh_hook_chain_setup_transit(rox);

        kh_mem_rox_write_disable(rox, sizeof(kh_hook_chain_rox_t));
        flush_code_cache(rox, sizeof(kh_hook_chain_rox_t));

        if (kh_mem_register_origin(func_addr, rox) != 0) {
            kh_mem_free_rox(rox, sizeof(kh_hook_chain_rox_t));
            kh_mem_free_rw(rw, sizeof(kh_hook_chain_rw_t));
            return HOOK_NO_MEM;
        }

        kh_hook_install(&rox->kh_hook);
    } else {
        rw = rox->rw;
        if (!rw)
            return HOOK_BAD_ADDRESS;
    }

    return kh_hook_chain_add(rw, before, after, udata, priority);
}

/* ---- Hook unwrap / remove ---- */

void kh_hook_unwrap_remove(void *func, void *before, void *after, int remove)
{
    if (!func)
        return;

    func = STRIP_PAC(func);
    uintptr_t func_addr = (uintptr_t)func;
    kh_hook_chain_rox_t *rox =
        (kh_hook_chain_rox_t *)kh_mem_get_rox_from_origin(func_addr);
    if (!rox || !rox->rw)
        return;

    kh_hook_chain_rw_t *rw = rox->rw;

    kh_hook_chain_remove(rw, before, after);

    if (remove && il_chain_all_empty(rw)) {
        kh_hook_uninstall(&rox->kh_hook);
        kh_mem_unregister_origin(func_addr);
        kh_mem_free_rw(rw, sizeof(kh_hook_chain_rw_t));
        kh_mem_free_rox(rox, sizeof(kh_hook_chain_rox_t));
    }
}

/* ==================================================================
 * Function pointer kh_hook API
 * ================================================================== */

static void write_fp_value(uintptr_t fp_addr, uintptr_t value)
{
    *(volatile uintptr_t *)fp_addr = value;
}

/* ---- Simple function pointer kh_hook (no chain) ---- */

void kh_fp_hook(uintptr_t fp_addr, void *replace, void **backup)
{
    if (!fp_addr || !replace || !backup)
        return;

    fp_addr = (uintptr_t)STRIP_PAC(fp_addr);
    *backup = *(void **)fp_addr;
    write_fp_value(fp_addr, (uintptr_t)replace);
}

void kh_fp_unhook(uintptr_t fp_addr, void *backup)
{
    if (!fp_addr)
        return;

    fp_addr = (uintptr_t)STRIP_PAC(fp_addr);
    write_fp_value(fp_addr, (uintptr_t)backup);
}

/* ---- Chain-based function pointer kh_hook ---- */

kh_hook_err_t kh_fp_hook_wrap(uintptr_t fp_addr, int32_t argno, void *before,
                        void *after, void *udata, int32_t priority)
{
    if (!fp_addr)
        return HOOK_BAD_ADDRESS;

    fp_addr = (uintptr_t)STRIP_PAC(fp_addr);
    kh_fp_hook_chain_rox_t *rox;
    kh_fp_hook_chain_rw_t *rw;

    rox = (kh_fp_hook_chain_rox_t *)kh_mem_get_rox_from_origin(fp_addr);

    if (!rox) {
        rox = (kh_fp_hook_chain_rox_t *)kh_mem_alloc_rox(sizeof(kh_fp_hook_chain_rox_t));
        if (!rox)
            return HOOK_NO_MEM;

        rw = (kh_fp_hook_chain_rw_t *)kh_mem_alloc_rw(sizeof(kh_fp_hook_chain_rw_t));
        if (!rw) {
            kh_mem_free_rox(rox, sizeof(kh_fp_hook_chain_rox_t));
            return HOOK_NO_MEM;
        }

        __builtin_memset(rw, 0, sizeof(kh_fp_hook_chain_rw_t));
        rw->rox = rox;
        rw->chain_items_max = FP_HOOK_CHAIN_NUM;
        rw->argno = argno;
        rw->sorted_count = 0;

        kh_mem_rox_write_enable(rox, sizeof(kh_fp_hook_chain_rox_t));

        rox->rw = rw;

        kh_fp_hook_t *h = &rox->kh_hook;
        h->fp_addr = fp_addr;
        h->origin_fp = *(uintptr_t *)fp_addr;
        h->replace_addr = (uintptr_t)&rox->transit[2];

        kh_fp_hook_chain_setup_transit(rox);

        kh_mem_rox_write_disable(rox, sizeof(kh_fp_hook_chain_rox_t));
        flush_code_cache(rox, sizeof(kh_fp_hook_chain_rox_t));

        if (kh_mem_register_origin(fp_addr, rox) != 0) {
            kh_mem_free_rox(rox, sizeof(kh_fp_hook_chain_rox_t));
            kh_mem_free_rw(rw, sizeof(kh_fp_hook_chain_rw_t));
            return HOOK_NO_MEM;
        }

        write_fp_value(fp_addr, h->replace_addr);
    } else {
        rw = rox->rw;
        if (!rw)
            return HOOK_BAD_ADDRESS;
    }

    return fp_chain_add(rw, before, after, udata, priority);
}

void kh_fp_hook_unwrap(uintptr_t fp_addr, void *before, void *after)
{
    if (!fp_addr)
        return;

    fp_addr = (uintptr_t)STRIP_PAC(fp_addr);
    kh_fp_hook_chain_rox_t *rox =
        (kh_fp_hook_chain_rox_t *)kh_mem_get_rox_from_origin(fp_addr);
    if (!rox || !rox->rw)
        return;

    kh_fp_hook_chain_rw_t *rw = rox->rw;

    fp_chain_remove(rw, before, after);

    if (fp_chain_all_empty(rw)) {
        write_fp_value(fp_addr, rox->kh_hook.origin_fp);
        kh_mem_unregister_origin(fp_addr);
        kh_mem_free_rw(rw, sizeof(kh_fp_hook_chain_rw_t));
        kh_mem_free_rox(rox, sizeof(kh_fp_hook_chain_rox_t));
    }
}
