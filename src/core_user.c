/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2026 bmax121.
 * Userspace hook chain API — adapts the kernel core logic for
 * userspace memory management and hook installation.
 */

#include <ktypes.h>
#include <hook.h>
#include <hmem.h>
#include <platform.h>
#include <log.h>

/* kp_log_func defined in src/platform/log_user.c */

/* ---- Generic chain operations (shared by inline and FP hooks) ----
 *
 * hook_chain_rw_t and fp_hook_chain_rw_t share the same field layout
 * for chain_items_max, items[], sorted_indices[], sorted_count.
 * Use macros to generate type-safe wrappers without code duplication.
 */

#define DEFINE_CHAIN_OPS(PREFIX, RW_TYPE)                                       \
                                                                                \
static void PREFIX##_rebuild_sorted(RW_TYPE *rw)                                \
{                                                                               \
    int32_t count = 0;                                                          \
    for (int32_t i = 0; i < rw->chain_items_max; i++) {                        \
        if (rw->items[i].state == CHAIN_ITEM_STATE_READY)                      \
            rw->sorted_indices[count++] = i;                                    \
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
static hook_err_t PREFIX##_chain_add(RW_TYPE *rw, void *before, void *after,    \
                                      void *udata, int32_t priority)            \
{                                                                               \
    if (!rw) return HOOK_BAD_ADDRESS;                                           \
    int32_t slot = -1;                                                          \
    for (int32_t i = 0; i < rw->chain_items_max; i++) {                        \
        if (rw->items[i].state == CHAIN_ITEM_STATE_EMPTY) { slot = i; break; } \
    }                                                                           \
    if (slot < 0) return HOOK_CHAIN_FULL;                                       \
    hook_chain_item_t *item = &rw->items[slot];                                 \
    item->state = CHAIN_ITEM_STATE_READY;                                       \
    item->priority = priority;                                                  \
    item->udata = udata;                                                        \
    item->before = before;                                                      \
    item->after = after;                                                        \
    __builtin_memset(&item->local, 0, sizeof(hook_local_t));                    \
    PREFIX##_rebuild_sorted(rw);                                                \
    return HOOK_NO_ERR;                                                         \
}                                                                               \
                                                                                \
static void PREFIX##_chain_remove(RW_TYPE *rw, void *before, void *after)       \
{                                                                               \
    if (!rw) return;                                                            \
    for (int32_t i = 0; i < rw->chain_items_max; i++) {                        \
        hook_chain_item_t *item = &rw->items[i];                                \
        if (item->state != CHAIN_ITEM_STATE_READY) continue;                   \
        if (item->before == before && item->after == after) {                   \
            item->state = CHAIN_ITEM_STATE_EMPTY;                               \
            item->before = 0;                                                   \
            item->after = 0;                                                    \
            item->udata = 0;                                                    \
            item->priority = 0;                                                 \
            break;                                                              \
        }                                                                       \
    }                                                                           \
    PREFIX##_rebuild_sorted(rw);                                                \
}                                                                               \
                                                                                \
static int PREFIX##_chain_all_empty(RW_TYPE *rw)                                \
{                                                                               \
    for (int32_t i = 0; i < rw->chain_items_max; i++) {                        \
        if (rw->items[i].state != CHAIN_ITEM_STATE_EMPTY) return 0;            \
    }                                                                           \
    return 1;                                                                   \
}

/* Generate inline hook chain ops (il_ prefix) */
DEFINE_CHAIN_OPS(il, hook_chain_rw_t)

/* Generate FP hook chain ops (fp_ prefix) */
DEFINE_CHAIN_OPS(fp, fp_hook_chain_rw_t)

/* Public API wrappers for inline hook chain */
hook_err_t hook_chain_add(hook_chain_rw_t *rw, void *before, void *after,
                          void *udata, int32_t priority)
{
    return il_chain_add(rw, before, after, udata, priority);
}

void hook_chain_remove(hook_chain_rw_t *rw, void *before, void *after)
{
    il_chain_remove(rw, before, after);
}

/* ---- Simple inline hook (no chain) ---- */

hook_err_t hook(void *func, void *replace, void **backup)
{
    if (!func || !replace || !backup)
        return HOOK_BAD_ADDRESS;

    uint64_t func_addr = (uint64_t)func;

    /* Duplicate check */
    if (hook_mem_get_rox_from_origin(func_addr))
        return HOOK_DUPLICATED;

    hook_chain_rox_t *rox =
        (hook_chain_rox_t *)hook_mem_alloc_rox(sizeof(hook_chain_rox_t));
    if (!rox)
        return HOOK_NO_MEM;

    hook_mem_rox_write_enable(rox, sizeof(hook_chain_rox_t));

    rox->rw = 0;

    hook_t *h = &rox->hook;
    h->func_addr = func_addr;
    h->origin_addr = func_addr;
    h->replace_addr = (uint64_t)replace;
    h->relo_addr = (uint64_t)h->relo_insts;
    h->tramp_insts_num = 0;
    h->relo_insts_num = 0;

    hook_err_t err = hook_prepare(h);
    if (err) {
        hook_mem_rox_write_disable(rox, sizeof(hook_chain_rox_t));
        hook_mem_free_rox(rox, sizeof(hook_chain_rox_t));
        return err;
    }

    hook_mem_rox_write_disable(rox, sizeof(hook_chain_rox_t));

    hook_install(h);
    hook_mem_register_origin(func_addr, rox);

    *backup = (void *)h->relo_addr;
    return HOOK_NO_ERR;
}

void unhook(void *func)
{
    if (!func)
        return;

    uint64_t func_addr = (uint64_t)func;
    hook_chain_rox_t *rox =
        (hook_chain_rox_t *)hook_mem_get_rox_from_origin(func_addr);
    if (!rox)
        return;

    hook_uninstall(&rox->hook);
    hook_mem_unregister_origin(func_addr);
    hook_mem_free_rox(rox, sizeof(hook_chain_rox_t));
}

/* ---- Chain-based inline hook (hook_wrap) ---- */

hook_err_t hook_wrap_pri(void *func, int32_t argno, void *before,
                         void *after, void *udata, int32_t priority)
{
    if (!func)
        return HOOK_BAD_ADDRESS;

    uint64_t func_addr = (uint64_t)func;
    hook_chain_rox_t *rox;
    hook_chain_rw_t *rw;

    /* Check if this function is already chain-hooked */
    rox = (hook_chain_rox_t *)hook_mem_get_rox_from_origin(func_addr);

    if (!rox) {
        /* First hook on this function — allocate and set up */
        rox = (hook_chain_rox_t *)hook_mem_alloc_rox(sizeof(hook_chain_rox_t));
        if (!rox)
            return HOOK_NO_MEM;

        rw = (hook_chain_rw_t *)hook_mem_alloc_rw(sizeof(hook_chain_rw_t));
        if (!rw) {
            hook_mem_free_rox(rox, sizeof(hook_chain_rox_t));
            return HOOK_NO_MEM;
        }

        /* Initialize RW side */
        __builtin_memset(rw, 0, sizeof(hook_chain_rw_t));
        rw->rox = rox;
        rw->chain_items_max = HOOK_CHAIN_NUM;
        rw->argno = argno;
        rw->sorted_count = 0;

        /* Enable writing to ROX memory for hook_prepare + transit setup */
        hook_mem_rox_write_enable(rox, sizeof(hook_chain_rox_t));

        rox->rw = rw;

        hook_t *h = &rox->hook;
        h->func_addr = func_addr;
        h->origin_addr = func_addr;
        h->replace_addr = (uint64_t)&rox->transit[2]; /* transit stub entry */
        h->relo_addr = (uint64_t)h->relo_insts;
        h->tramp_insts_num = 0;
        h->relo_insts_num = 0;

        hook_err_t err = hook_prepare(h);
        if (err) {
            hook_mem_rox_write_disable(rox, sizeof(hook_chain_rox_t));
            hook_mem_free_rox(rox, sizeof(hook_chain_rox_t));
            hook_mem_free_rw(rw, sizeof(hook_chain_rw_t));
            return err;
        }

        /* Set up transit buffer (self-pointer + asm stub copy) */
        hook_chain_setup_transit(rox);

        hook_mem_rox_write_disable(rox, sizeof(hook_chain_rox_t));

        /* Register for lookup by func_addr */
        hook_mem_register_origin(func_addr, rox);

        /* Patch the target function */
        hook_install(&rox->hook);
    } else {
        rw = rox->rw;
        if (!rw)
            return HOOK_BAD_ADDRESS;
    }

    return hook_chain_add(rw, before, after, udata, priority);
}

/* ---- Hook unwrap / remove ---- */

void hook_unwrap_remove(void *func, void *before, void *after, int remove)
{
    if (!func)
        return;

    uint64_t func_addr = (uint64_t)func;
    hook_chain_rox_t *rox =
        (hook_chain_rox_t *)hook_mem_get_rox_from_origin(func_addr);
    if (!rox || !rox->rw)
        return;

    hook_chain_rw_t *rw = rox->rw;

    hook_chain_remove(rw, before, after);

    /* If all chain items are empty and caller wants removal, tear down */
    if (remove && il_chain_all_empty(rw)) {
        hook_uninstall(&rox->hook);
        hook_mem_unregister_origin(func_addr);
        hook_mem_free_rw(rw, sizeof(hook_chain_rw_t));
        hook_mem_free_rox(rox, sizeof(hook_chain_rox_t));
    }
}

/* ==================================================================
 * Function pointer hook API
 * ================================================================== */

/* ---- Helper: write a value to a function pointer address ---- */

static void write_fp_value(uintptr_t fp_addr, uint64_t value)
{
    /* In userspace, function pointers typically live in writable data
     * segments (.data / .bss).  Temporarily making the page RW then
     * restoring to RO would break other globals on the same page.
     * Just write directly — if the page happens to be read-only (e.g.
     * rodata), the caller must handle protection changes. */
    *(volatile uint64_t *)fp_addr = value;
}

/* ---- Simple function pointer hook (no chain) ---- */

void fp_hook(uintptr_t fp_addr, void *replace, void **backup)
{
    if (!fp_addr || !replace || !backup)
        return;

    *backup = *(void **)fp_addr;
    write_fp_value(fp_addr, (uint64_t)replace);
}

void fp_unhook(uintptr_t fp_addr, void *backup)
{
    if (!fp_addr)
        return;

    write_fp_value(fp_addr, (uint64_t)backup);
}

/* ---- Chain-based function pointer hook ---- */

hook_err_t fp_hook_wrap_pri(uintptr_t fp_addr, int32_t argno, void *before,
                             void *after, void *udata, int32_t priority)
{
    if (!fp_addr)
        return HOOK_BAD_ADDRESS;

    fp_hook_chain_rox_t *rox;
    fp_hook_chain_rw_t *rw;

    /* Check if this function pointer is already chain-hooked */
    rox = (fp_hook_chain_rox_t *)hook_mem_get_rox_from_origin(fp_addr);

    if (!rox) {
        /* First hook on this FP — allocate and set up */
        rox = (fp_hook_chain_rox_t *)hook_mem_alloc_rox(sizeof(fp_hook_chain_rox_t));
        if (!rox)
            return HOOK_NO_MEM;

        rw = (fp_hook_chain_rw_t *)hook_mem_alloc_rw(sizeof(fp_hook_chain_rw_t));
        if (!rw) {
            hook_mem_free_rox(rox, sizeof(fp_hook_chain_rox_t));
            return HOOK_NO_MEM;
        }

        /* Initialize RW side */
        __builtin_memset(rw, 0, sizeof(fp_hook_chain_rw_t));
        rw->rox = rox;
        rw->chain_items_max = FP_HOOK_CHAIN_NUM;
        rw->argno = argno;
        rw->sorted_count = 0;

        /* Enable writing to ROX memory for transit setup */
        hook_mem_rox_write_enable(rox, sizeof(fp_hook_chain_rox_t));

        rox->rw = rw;

        fp_hook_t *h = &rox->hook;
        h->fp_addr = fp_addr;
        h->origin_fp = *(uint64_t *)fp_addr;
        h->replace_addr = (uint64_t)&rox->transit[2];

        /* Set up transit buffer (self-pointer + FP asm stub copy) */
        fp_hook_chain_setup_transit(rox);

        hook_mem_rox_write_disable(rox, sizeof(fp_hook_chain_rox_t));

        /* Register for lookup by fp_addr */
        hook_mem_register_origin(fp_addr, rox);

        /* Swap the function pointer to point to transit stub */
        write_fp_value(fp_addr, h->replace_addr);
    } else {
        rw = rox->rw;
        if (!rw)
            return HOOK_BAD_ADDRESS;
    }

    return fp_chain_add(rw, before, after, udata, priority);
}

void fp_hook_unwrap(uintptr_t fp_addr, void *before, void *after)
{
    if (!fp_addr)
        return;

    fp_hook_chain_rox_t *rox =
        (fp_hook_chain_rox_t *)hook_mem_get_rox_from_origin(fp_addr);
    if (!rox || !rox->rw)
        return;

    fp_hook_chain_rw_t *rw = rox->rw;

    fp_chain_remove(rw, before, after);

    /* If all chain items are empty, restore original FP and tear down */
    if (fp_chain_all_empty(rw)) {
        write_fp_value(fp_addr, rox->hook.origin_fp);
        hook_mem_unregister_origin(fp_addr);
        hook_mem_free_rw(rw, sizeof(fp_hook_chain_rw_t));
        hook_mem_free_rox(rox, sizeof(fp_hook_chain_rox_t));
    }
}
