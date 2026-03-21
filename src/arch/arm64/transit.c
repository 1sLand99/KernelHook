/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2023 bmax121. All Rights Reserved.
 * ARM64 transit functions with ROX/RW split, priority ordering, and per-item local isolation.
 */

#include <ktypes.h>
#include <hook.h>

static inline void transit_memset(void *dst, int val, size_t n)
{
    uint8_t *d = (uint8_t *)dst;
    for (size_t i = 0; i < n; i++)
        d[i] = (uint8_t)val;
}

/*
 * Macro to find the hook_chain_rox_t base from within a transit function.
 *
 * The transit function is copied into rox->transit[2..] at runtime.
 * transit[0] = ARM64_BTI_JC, transit[1] = ARM64_NOP.
 * The adr inline asm gives us an address within transit[].
 * We walk backward to find the NOP sentinel (transit[1]), then
 * step back one more to transit[0], and use offsetof to find rox base.
 */
#define FIND_ROX_AND_RW(rox_var, rw_var)                                                    \
    uint64_t _this_va;                                                                       \
    asm volatile("adr %0, ." : "=r"(_this_va));                                              \
    uint32_t *_vptr = (uint32_t *)_this_va;                                                  \
    while (*--_vptr != ARM64_NOP) {                                                          \
    };                                                                                       \
    _vptr--;                                                                                 \
    hook_chain_rox_t *rox_var =                                                              \
        local_container_of((uint64_t)_vptr, hook_chain_rox_t, transit);                      \
    hook_chain_rw_t *rw_var = rox_var->rw

/*
 * Macro to clear locals for all active chain items at transit entry.
 */
#define CLEAR_ACTIVE_LOCALS(rw)                                                              \
    do {                                                                                     \
        for (int32_t _ci = 0; _ci < (rw)->sorted_count; _ci++) {                            \
            int32_t _idx = (rw)->sorted_indices[_ci];                                        \
            transit_memset(&(rw)->locals[_idx], 0, sizeof(hook_local_t));                    \
        }                                                                                    \
    } while (0)

/*
 * Macro to call before callbacks in priority order (forward through sorted_indices).
 */
#define CALL_BEFORES(rw, fargs_ptr, cb_type)                                                 \
    do {                                                                                     \
        for (int32_t _si = 0; _si < (rw)->sorted_count; _si++) {                            \
            int32_t _idx = (rw)->sorted_indices[_si];                                        \
            if ((rw)->states[_idx] != CHAIN_ITEM_STATE_READY) continue;                      \
            (fargs_ptr)->local = &(rw)->locals[_idx];                                        \
            cb_type _func = (cb_type)(rw)->befores[_idx];                                    \
            if (_func) _func((fargs_ptr), (rw)->udata[_idx]);                                \
        }                                                                                    \
    } while (0)

/*
 * Macro to call after callbacks in reverse priority order (reverse through sorted_indices).
 */
#define CALL_AFTERS(rw, fargs_ptr, cb_type)                                                  \
    do {                                                                                     \
        for (int32_t _si = (rw)->sorted_count - 1; _si >= 0; _si--) {                       \
            int32_t _idx = (rw)->sorted_indices[_si];                                        \
            if ((rw)->states[_idx] != CHAIN_ITEM_STATE_READY) continue;                      \
            (fargs_ptr)->local = &(rw)->locals[_idx];                                        \
            cb_type _func = (cb_type)(rw)->afters[_idx];                                     \
            if (_func) _func((fargs_ptr), (rw)->udata[_idx]);                                \
        }                                                                                    \
    } while (0)

/* ---- transit0: no arguments ---- */

typedef uint64_t (*transit0_func_t)();

uint64_t __attribute__((section(".transit0.text"))) __attribute__((__noinline__)) _transit0()
{
    FIND_ROX_AND_RW(rox, rw);

    hook_fargs0_t fargs;
    fargs.skip_origin = 0;
    fargs.chain = rox;
    fargs.local = NULL;
    fargs.ret = 0;

    CLEAR_ACTIVE_LOCALS(rw);
    CALL_BEFORES(rw, &fargs, hook_chain0_callback);

    if (!fargs.skip_origin) {
        transit0_func_t origin_func = (transit0_func_t)rox->hook.relo_addr;
        fargs.ret = origin_func();
    }

    CALL_AFTERS(rw, &fargs, hook_chain0_callback);

    return fargs.ret;
}
extern void _transit0_end();

/* ---- transit4: 4 arguments (covers argno 1-4) ---- */

typedef uint64_t (*transit4_func_t)(uint64_t, uint64_t, uint64_t, uint64_t);

uint64_t __attribute__((section(".transit4.text"))) __attribute__((__noinline__))
_transit4(uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t arg3)
{
    FIND_ROX_AND_RW(rox, rw);

    hook_fargs4_t fargs;
    fargs.skip_origin = 0;
    fargs.chain = rox;
    fargs.local = NULL;
    fargs.ret = 0;
    fargs.arg0 = arg0;
    fargs.arg1 = arg1;
    fargs.arg2 = arg2;
    fargs.arg3 = arg3;

    CLEAR_ACTIVE_LOCALS(rw);
    CALL_BEFORES(rw, &fargs, hook_chain4_callback);

    if (!fargs.skip_origin) {
        transit4_func_t origin_func = (transit4_func_t)rox->hook.relo_addr;
        fargs.ret = origin_func(fargs.arg0, fargs.arg1, fargs.arg2, fargs.arg3);
    }

    CALL_AFTERS(rw, &fargs, hook_chain4_callback);

    return fargs.ret;
}
extern void _transit4_end();

/* ---- transit8: 8 arguments (covers argno 5-8) ---- */

typedef uint64_t (*transit8_func_t)(uint64_t, uint64_t, uint64_t, uint64_t,
                                    uint64_t, uint64_t, uint64_t, uint64_t);

uint64_t __attribute__((section(".transit8.text"))) __attribute__((__noinline__))
_transit8(uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t arg3,
          uint64_t arg4, uint64_t arg5, uint64_t arg6, uint64_t arg7)
{
    FIND_ROX_AND_RW(rox, rw);

    hook_fargs8_t fargs;
    fargs.skip_origin = 0;
    fargs.chain = rox;
    fargs.local = NULL;
    fargs.ret = 0;
    fargs.arg0 = arg0;
    fargs.arg1 = arg1;
    fargs.arg2 = arg2;
    fargs.arg3 = arg3;
    fargs.arg4 = arg4;
    fargs.arg5 = arg5;
    fargs.arg6 = arg6;
    fargs.arg7 = arg7;

    CLEAR_ACTIVE_LOCALS(rw);
    CALL_BEFORES(rw, &fargs, hook_chain8_callback);

    if (!fargs.skip_origin) {
        transit8_func_t origin_func = (transit8_func_t)rox->hook.relo_addr;
        fargs.ret = origin_func(fargs.arg0, fargs.arg1, fargs.arg2, fargs.arg3,
                                fargs.arg4, fargs.arg5, fargs.arg6, fargs.arg7);
    }

    CALL_AFTERS(rw, &fargs, hook_chain8_callback);

    return fargs.ret;
}
extern void _transit8_end();

/* ---- transit12: 12 arguments (covers argno 9-12) ---- */

typedef uint64_t (*transit12_func_t)(uint64_t, uint64_t, uint64_t, uint64_t,
                                     uint64_t, uint64_t, uint64_t, uint64_t,
                                     uint64_t, uint64_t, uint64_t, uint64_t);

uint64_t __attribute__((section(".transit12.text"))) __attribute__((__noinline__))
_transit12(uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t arg3,
           uint64_t arg4, uint64_t arg5, uint64_t arg6, uint64_t arg7,
           uint64_t arg8, uint64_t arg9, uint64_t arg10, uint64_t arg11)
{
    FIND_ROX_AND_RW(rox, rw);

    hook_fargs12_t fargs;
    fargs.skip_origin = 0;
    fargs.chain = rox;
    fargs.local = NULL;
    fargs.ret = 0;
    fargs.arg0 = arg0;
    fargs.arg1 = arg1;
    fargs.arg2 = arg2;
    fargs.arg3 = arg3;
    fargs.arg4 = arg4;
    fargs.arg5 = arg5;
    fargs.arg6 = arg6;
    fargs.arg7 = arg7;
    fargs.arg8 = arg8;
    fargs.arg9 = arg9;
    fargs.arg10 = arg10;
    fargs.arg11 = arg11;

    CLEAR_ACTIVE_LOCALS(rw);
    CALL_BEFORES(rw, &fargs, hook_chain12_callback);

    if (!fargs.skip_origin) {
        transit12_func_t origin_func = (transit12_func_t)rox->hook.relo_addr;
        fargs.ret = origin_func(fargs.arg0, fargs.arg1, fargs.arg2, fargs.arg3,
                                fargs.arg4, fargs.arg5, fargs.arg6, fargs.arg7,
                                fargs.arg8, fargs.arg9, fargs.arg10, fargs.arg11);
    }

    CALL_AFTERS(rw, &fargs, hook_chain12_callback);

    return fargs.ret;
}
extern void _transit12_end();
