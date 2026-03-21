/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2023 bmax121. All Rights Reserved.
 */

#ifndef _KP_HOOK_H_
#define _KP_HOOK_H_

#include <ktypes.h>

#define HOOK_INTO_BRANCH_FUNC

typedef enum
{
    HOOK_NO_ERR = 0,
    HOOK_BAD_ADDRESS = 4095,
    HOOK_DUPLICATED = 4094,
    HOOK_NO_MEM = 4093,
    HOOK_BAD_RELO = 4092,
    HOOK_TRANSIT_NO_MEM = 4091,
    HOOK_CHAIN_FULL = 4090,
} hook_err_t;

enum hook_type
{
    NONE = 0,
    INLINE,
    INLINE_CHAIN,
    FUNCTION_POINTER_CHAIN,
};

typedef int8_t chain_item_state;

#define CHAIN_ITEM_STATE_EMPTY 0
#define CHAIN_ITEM_STATE_READY 1
#define CHAIN_ITEM_STATE_BUSY 2

#define local_offsetof(TYPE, MEMBER) ((size_t) & ((TYPE *)0)->MEMBER)
#define local_container_of(ptr, type, member) ({ (type *)((char *)(ptr) - local_offsetof(type, member)); })

#define HOOK_MEM_REGION_NUM 4
#define TRAMPOLINE_NUM 4
#define RELOCATE_INST_NUM (TRAMPOLINE_NUM * 8 + 8)

#define HOOK_CHAIN_NUM 0x10
#define TRANSIT_INST_NUM 0x60

#define FP_HOOK_CHAIN_NUM 0x20

#define ARM64_NOP 0xd503201f
#define ARM64_BTI_C 0xd503245f
#define ARM64_BTI_J 0xd503249f
#define ARM64_BTI_JC 0xd50324df

#define HOOK_LOCAL_DATA_NUM 8

/* ---- Core hook_t (inline hook state, unchanged) ---- */

typedef struct
{
    /* in */
    uint64_t func_addr;
    uint64_t origin_addr;
    uint64_t replace_addr;
    uint64_t relo_addr;
    /* out */
    int32_t tramp_insts_num;
    int32_t relo_insts_num;
    uint32_t origin_insts[TRAMPOLINE_NUM] __aligned(8);
    uint32_t tramp_insts[TRAMPOLINE_NUM] __aligned(8);
    uint32_t relo_insts[RELOCATE_INST_NUM] __aligned(8);
} hook_t __aligned(8);

/* ---- Per-item local storage ---- */

typedef struct
{
    union
    {
        struct
        {
            uint64_t data0;
            uint64_t data1;
            uint64_t data2;
            uint64_t data3;
            uint64_t data4;
            uint64_t data5;
            uint64_t data6;
            uint64_t data7;
        };
        uint64_t data[HOOK_LOCAL_DATA_NUM];
    };
} hook_local_t;

/* ---- Hook fargs: local is now a pointer ---- */

typedef struct
{
    void *chain;
    int skip_origin;
    hook_local_t *local;
    uint64_t ret;
    union
    {
        struct
        {
        };
        uint64_t args[0];
    };
} hook_fargs0_t __aligned(8);

typedef struct
{
    void *chain;
    int skip_origin;
    hook_local_t *local;
    uint64_t ret;
    union
    {
        struct
        {
            uint64_t arg0;
            uint64_t arg1;
            uint64_t arg2;
            uint64_t arg3;
        };
        uint64_t args[4];
    };
} hook_fargs4_t __aligned(8);

typedef hook_fargs4_t hook_fargs1_t;
typedef hook_fargs4_t hook_fargs2_t;
typedef hook_fargs4_t hook_fargs3_t;

typedef struct
{
    void *chain;
    int skip_origin;
    hook_local_t *local;
    uint64_t ret;
    union
    {
        struct
        {
            uint64_t arg0;
            uint64_t arg1;
            uint64_t arg2;
            uint64_t arg3;
            uint64_t arg4;
            uint64_t arg5;
            uint64_t arg6;
            uint64_t arg7;
        };
        uint64_t args[8];
    };
} hook_fargs8_t __aligned(8);

typedef hook_fargs8_t hook_fargs5_t;
typedef hook_fargs8_t hook_fargs6_t;
typedef hook_fargs8_t hook_fargs7_t;

typedef struct
{
    void *chain;
    int skip_origin;
    hook_local_t *local;
    uint64_t ret;
    union
    {
        struct
        {
            uint64_t arg0;
            uint64_t arg1;
            uint64_t arg2;
            uint64_t arg3;
            uint64_t arg4;
            uint64_t arg5;
            uint64_t arg6;
            uint64_t arg7;
            uint64_t arg8;
            uint64_t arg9;
            uint64_t arg10;
            uint64_t arg11;
        };
        uint64_t args[12];
    };
} hook_fargs12_t __aligned(8);

typedef hook_fargs12_t hook_fargs9_t;
typedef hook_fargs12_t hook_fargs10_t;
typedef hook_fargs12_t hook_fargs11_t;

/* ---- Callback typedefs ---- */

typedef void (*hook_chain0_callback)(hook_fargs0_t *fargs, void *udata);
typedef void (*hook_chain1_callback)(hook_fargs1_t *fargs, void *udata);
typedef void (*hook_chain2_callback)(hook_fargs2_t *fargs, void *udata);
typedef void (*hook_chain3_callback)(hook_fargs3_t *fargs, void *udata);
typedef void (*hook_chain4_callback)(hook_fargs4_t *fargs, void *udata);
typedef void (*hook_chain5_callback)(hook_fargs5_t *fargs, void *udata);
typedef void (*hook_chain6_callback)(hook_fargs6_t *fargs, void *udata);
typedef void (*hook_chain7_callback)(hook_fargs7_t *fargs, void *udata);
typedef void (*hook_chain8_callback)(hook_fargs8_t *fargs, void *udata);
typedef void (*hook_chain9_callback)(hook_fargs9_t *fargs, void *udata);
typedef void (*hook_chain10_callback)(hook_fargs10_t *fargs, void *udata);
typedef void (*hook_chain11_callback)(hook_fargs11_t *fargs, void *udata);
typedef void (*hook_chain12_callback)(hook_fargs12_t *fargs, void *udata);

/* ---- ROX/RW split: inline hook chain ---- */

struct hook_chain_rw;

typedef struct
{
    hook_t hook;
    struct hook_chain_rw *rw;
    uint32_t transit[TRANSIT_INST_NUM];
} hook_chain_rox_t __aligned(64);

typedef struct hook_chain_rw
{
    hook_chain_rox_t *rox;
    int32_t chain_items_max;
    int32_t sorted_indices[HOOK_CHAIN_NUM];
    int32_t sorted_count;
    chain_item_state states[HOOK_CHAIN_NUM];
    int32_t priorities[HOOK_CHAIN_NUM];
    void *udata[HOOK_CHAIN_NUM];
    void *befores[HOOK_CHAIN_NUM];
    void *afters[HOOK_CHAIN_NUM];
    hook_local_t locals[HOOK_CHAIN_NUM];
} hook_chain_rw_t __aligned(8);

/* ---- Function pointer hook ---- */

typedef struct
{
    uintptr_t fp_addr;
    uint64_t replace_addr;
    uint64_t origin_fp;
} fp_hook_t __aligned(8);

/* ---- ROX/RW split: function pointer hook chain ---- */

struct fp_hook_chain_rw;

typedef struct
{
    fp_hook_t hook;
    struct fp_hook_chain_rw *rw;
    uint32_t transit[TRANSIT_INST_NUM];
} fp_hook_chain_rox_t __aligned(64);

typedef struct fp_hook_chain_rw
{
    fp_hook_chain_rox_t *rox;
    int32_t chain_items_max;
    int32_t sorted_indices[FP_HOOK_CHAIN_NUM];
    int32_t sorted_count;
    chain_item_state states[FP_HOOK_CHAIN_NUM];
    int32_t priorities[FP_HOOK_CHAIN_NUM];
    void *udata[FP_HOOK_CHAIN_NUM];
    void *befores[FP_HOOK_CHAIN_NUM];
    void *afters[FP_HOOK_CHAIN_NUM];
    hook_local_t locals[FP_HOOK_CHAIN_NUM];
} fp_hook_chain_rw_t __aligned(8);

/* ---- Utility ---- */

static inline int is_bad_address(void *addr)
{
    return ((uint64_t)addr & 0x8000000000000000) != 0x8000000000000000;
}

/* ---- Instruction helpers (ARM64) ---- */

int32_t branch_from_to(uint32_t *tramp_buf, uint64_t src_addr, uint64_t dst_addr);
int32_t branch_relative(uint32_t *buf, uint64_t src_addr, uint64_t dst_addr);
int32_t branch_absolute(uint32_t *buf, uint64_t addr);
int32_t ret_absolute(uint32_t *buf, uint64_t addr);

/* ---- Hook prepare / install / uninstall ---- */

hook_err_t hook_prepare(hook_t *hook);
void hook_install(hook_t *hook);
void hook_uninstall(hook_t *hook);

/* ---- Inline hook API ---- */

hook_err_t hook(void *func, void *replace, void **backup);
void unhook(void *func);

hook_err_t hook_chain_add(hook_chain_rw_t *rw, void *before, void *after, void *udata, int32_t priority);
void hook_chain_remove(hook_chain_rw_t *rw, void *before, void *after);

hook_err_t hook_wrap_pri(void *func, int32_t argno, void *before, void *after, void *udata, int32_t priority);

static inline hook_err_t hook_wrap(void *func, int32_t argno, void *before, void *after, void *udata)
{
    return hook_wrap_pri(func, argno, before, after, udata, 0);
}

void hook_unwrap_remove(void *func, void *before, void *after, int remove);

static inline void hook_unwrap(void *func, void *before, void *after)
{
    hook_unwrap_remove(func, before, after, 1);
}

/* ---- Origin function access ---- */

static inline void *wrap_get_origin_func(void *hook_args)
{
    hook_fargs0_t *args = (hook_fargs0_t *)hook_args;
    hook_chain_rox_t *rox = (hook_chain_rox_t *)args->chain;
    return (void *)rox->hook.relo_addr;
}

/* ---- Function pointer hook API ---- */

void fp_hook(uintptr_t fp_addr, void *replace, void **backup);
void fp_unhook(uintptr_t fp_addr, void *backup);

hook_err_t fp_hook_wrap_pri(uintptr_t fp_addr, int32_t argno, void *before, void *after, void *udata, int32_t priority);

static inline hook_err_t fp_hook_wrap(uintptr_t fp_addr, int32_t argno, void *before, void *after, void *udata)
{
    return fp_hook_wrap_pri(fp_addr, argno, before, after, udata, 0);
}

void fp_hook_unwrap(uintptr_t fp_addr, void *before, void *after);

static inline void *fp_get_origin_func(void *hook_args)
{
    hook_fargs0_t *args = (hook_fargs0_t *)hook_args;
    fp_hook_chain_rox_t *rox = (fp_hook_chain_rox_t *)args->chain;
    return (void *)rox->hook.origin_fp;
}

/* ---- Chain install/uninstall helpers ---- */

static inline void hook_chain_install(hook_chain_rox_t *rox)
{
    hook_install(&rox->hook);
}

static inline void hook_chain_uninstall(hook_chain_rox_t *rox)
{
    hook_uninstall(&rox->hook);
}

/* ---- Typed convenience wrappers: hook_wrap0-12 ---- */

static inline hook_err_t hook_wrap0(void *func, hook_chain0_callback before, hook_chain0_callback after, void *udata)
{
    return hook_wrap(func, 0, (void *)before, (void *)after, udata);
}

static inline hook_err_t hook_wrap1(void *func, hook_chain1_callback before, hook_chain1_callback after, void *udata)
{
    return hook_wrap(func, 1, (void *)before, (void *)after, udata);
}

static inline hook_err_t hook_wrap2(void *func, hook_chain2_callback before, hook_chain2_callback after, void *udata)
{
    return hook_wrap(func, 2, (void *)before, (void *)after, udata);
}

static inline hook_err_t hook_wrap3(void *func, hook_chain3_callback before, hook_chain3_callback after, void *udata)
{
    return hook_wrap(func, 3, (void *)before, (void *)after, udata);
}

static inline hook_err_t hook_wrap4(void *func, hook_chain4_callback before, hook_chain4_callback after, void *udata)
{
    return hook_wrap(func, 4, (void *)before, (void *)after, udata);
}

static inline hook_err_t hook_wrap5(void *func, hook_chain5_callback before, hook_chain5_callback after, void *udata)
{
    return hook_wrap(func, 5, (void *)before, (void *)after, udata);
}

static inline hook_err_t hook_wrap6(void *func, hook_chain6_callback before, hook_chain6_callback after, void *udata)
{
    return hook_wrap(func, 6, (void *)before, (void *)after, udata);
}

static inline hook_err_t hook_wrap7(void *func, hook_chain7_callback before, hook_chain7_callback after, void *udata)
{
    return hook_wrap(func, 7, (void *)before, (void *)after, udata);
}

static inline hook_err_t hook_wrap8(void *func, hook_chain8_callback before, hook_chain8_callback after, void *udata)
{
    return hook_wrap(func, 8, (void *)before, (void *)after, udata);
}

static inline hook_err_t hook_wrap9(void *func, hook_chain9_callback before, hook_chain9_callback after, void *udata)
{
    return hook_wrap(func, 9, (void *)before, (void *)after, udata);
}

static inline hook_err_t hook_wrap10(void *func, hook_chain10_callback before, hook_chain10_callback after, void *udata)
{
    return hook_wrap(func, 10, (void *)before, (void *)after, udata);
}

static inline hook_err_t hook_wrap11(void *func, hook_chain11_callback before, hook_chain11_callback after, void *udata)
{
    return hook_wrap(func, 11, (void *)before, (void *)after, udata);
}

static inline hook_err_t hook_wrap12(void *func, hook_chain12_callback before, hook_chain12_callback after, void *udata)
{
    return hook_wrap(func, 12, (void *)before, (void *)after, udata);
}

/* ---- Typed convenience wrappers with priority: hook_wrap_pri0-12 ---- */

static inline hook_err_t hook_wrap_pri0(void *func, hook_chain0_callback before, hook_chain0_callback after, void *udata, int32_t priority)
{
    return hook_wrap_pri(func, 0, (void *)before, (void *)after, udata, priority);
}

static inline hook_err_t hook_wrap_pri1(void *func, hook_chain1_callback before, hook_chain1_callback after, void *udata, int32_t priority)
{
    return hook_wrap_pri(func, 1, (void *)before, (void *)after, udata, priority);
}

static inline hook_err_t hook_wrap_pri2(void *func, hook_chain2_callback before, hook_chain2_callback after, void *udata, int32_t priority)
{
    return hook_wrap_pri(func, 2, (void *)before, (void *)after, udata, priority);
}

static inline hook_err_t hook_wrap_pri3(void *func, hook_chain3_callback before, hook_chain3_callback after, void *udata, int32_t priority)
{
    return hook_wrap_pri(func, 3, (void *)before, (void *)after, udata, priority);
}

static inline hook_err_t hook_wrap_pri4(void *func, hook_chain4_callback before, hook_chain4_callback after, void *udata, int32_t priority)
{
    return hook_wrap_pri(func, 4, (void *)before, (void *)after, udata, priority);
}

static inline hook_err_t hook_wrap_pri5(void *func, hook_chain5_callback before, hook_chain5_callback after, void *udata, int32_t priority)
{
    return hook_wrap_pri(func, 5, (void *)before, (void *)after, udata, priority);
}

static inline hook_err_t hook_wrap_pri6(void *func, hook_chain6_callback before, hook_chain6_callback after, void *udata, int32_t priority)
{
    return hook_wrap_pri(func, 6, (void *)before, (void *)after, udata, priority);
}

static inline hook_err_t hook_wrap_pri7(void *func, hook_chain7_callback before, hook_chain7_callback after, void *udata, int32_t priority)
{
    return hook_wrap_pri(func, 7, (void *)before, (void *)after, udata, priority);
}

static inline hook_err_t hook_wrap_pri8(void *func, hook_chain8_callback before, hook_chain8_callback after, void *udata, int32_t priority)
{
    return hook_wrap_pri(func, 8, (void *)before, (void *)after, udata, priority);
}

static inline hook_err_t hook_wrap_pri9(void *func, hook_chain9_callback before, hook_chain9_callback after, void *udata, int32_t priority)
{
    return hook_wrap_pri(func, 9, (void *)before, (void *)after, udata, priority);
}

static inline hook_err_t hook_wrap_pri10(void *func, hook_chain10_callback before, hook_chain10_callback after, void *udata, int32_t priority)
{
    return hook_wrap_pri(func, 10, (void *)before, (void *)after, udata, priority);
}

static inline hook_err_t hook_wrap_pri11(void *func, hook_chain11_callback before, hook_chain11_callback after, void *udata, int32_t priority)
{
    return hook_wrap_pri(func, 11, (void *)before, (void *)after, udata, priority);
}

static inline hook_err_t hook_wrap_pri12(void *func, hook_chain12_callback before, hook_chain12_callback after, void *udata, int32_t priority)
{
    return hook_wrap_pri(func, 12, (void *)before, (void *)after, udata, priority);
}

/* ---- FP typed convenience wrappers: fp_hook_wrap0-12 ---- */

static inline hook_err_t fp_hook_wrap0(uintptr_t fp_addr, hook_chain0_callback before, hook_chain0_callback after, void *udata)
{
    return fp_hook_wrap(fp_addr, 0, (void *)before, (void *)after, udata);
}

static inline hook_err_t fp_hook_wrap1(uintptr_t fp_addr, hook_chain1_callback before, hook_chain1_callback after, void *udata)
{
    return fp_hook_wrap(fp_addr, 1, (void *)before, (void *)after, udata);
}

static inline hook_err_t fp_hook_wrap2(uintptr_t fp_addr, hook_chain2_callback before, hook_chain2_callback after, void *udata)
{
    return fp_hook_wrap(fp_addr, 2, (void *)before, (void *)after, udata);
}

static inline hook_err_t fp_hook_wrap3(uintptr_t fp_addr, hook_chain3_callback before, hook_chain3_callback after, void *udata)
{
    return fp_hook_wrap(fp_addr, 3, (void *)before, (void *)after, udata);
}

static inline hook_err_t fp_hook_wrap4(uintptr_t fp_addr, hook_chain4_callback before, hook_chain4_callback after, void *udata)
{
    return fp_hook_wrap(fp_addr, 4, (void *)before, (void *)after, udata);
}

static inline hook_err_t fp_hook_wrap5(uintptr_t fp_addr, hook_chain5_callback before, hook_chain5_callback after, void *udata)
{
    return fp_hook_wrap(fp_addr, 5, (void *)before, (void *)after, udata);
}

static inline hook_err_t fp_hook_wrap6(uintptr_t fp_addr, hook_chain6_callback before, hook_chain6_callback after, void *udata)
{
    return fp_hook_wrap(fp_addr, 6, (void *)before, (void *)after, udata);
}

static inline hook_err_t fp_hook_wrap7(uintptr_t fp_addr, hook_chain7_callback before, hook_chain7_callback after, void *udata)
{
    return fp_hook_wrap(fp_addr, 7, (void *)before, (void *)after, udata);
}

static inline hook_err_t fp_hook_wrap8(uintptr_t fp_addr, hook_chain8_callback before, hook_chain8_callback after, void *udata)
{
    return fp_hook_wrap(fp_addr, 8, (void *)before, (void *)after, udata);
}

static inline hook_err_t fp_hook_wrap9(uintptr_t fp_addr, hook_chain9_callback before, hook_chain9_callback after, void *udata)
{
    return fp_hook_wrap(fp_addr, 9, (void *)before, (void *)after, udata);
}

static inline hook_err_t fp_hook_wrap10(uintptr_t fp_addr, hook_chain10_callback before, hook_chain10_callback after, void *udata)
{
    return fp_hook_wrap(fp_addr, 10, (void *)before, (void *)after, udata);
}

static inline hook_err_t fp_hook_wrap11(uintptr_t fp_addr, hook_chain11_callback before, hook_chain11_callback after, void *udata)
{
    return fp_hook_wrap(fp_addr, 11, (void *)before, (void *)after, udata);
}

static inline hook_err_t fp_hook_wrap12(uintptr_t fp_addr, hook_chain12_callback before, hook_chain12_callback after, void *udata)
{
    return fp_hook_wrap(fp_addr, 12, (void *)before, (void *)after, udata);
}

/* ---- FP typed convenience wrappers with priority: fp_hook_wrap_pri0-12 ---- */

static inline hook_err_t fp_hook_wrap_pri0(uintptr_t fp_addr, hook_chain0_callback before, hook_chain0_callback after, void *udata, int32_t priority)
{
    return fp_hook_wrap_pri(fp_addr, 0, (void *)before, (void *)after, udata, priority);
}

static inline hook_err_t fp_hook_wrap_pri1(uintptr_t fp_addr, hook_chain1_callback before, hook_chain1_callback after, void *udata, int32_t priority)
{
    return fp_hook_wrap_pri(fp_addr, 1, (void *)before, (void *)after, udata, priority);
}

static inline hook_err_t fp_hook_wrap_pri2(uintptr_t fp_addr, hook_chain2_callback before, hook_chain2_callback after, void *udata, int32_t priority)
{
    return fp_hook_wrap_pri(fp_addr, 2, (void *)before, (void *)after, udata, priority);
}

static inline hook_err_t fp_hook_wrap_pri3(uintptr_t fp_addr, hook_chain3_callback before, hook_chain3_callback after, void *udata, int32_t priority)
{
    return fp_hook_wrap_pri(fp_addr, 3, (void *)before, (void *)after, udata, priority);
}

static inline hook_err_t fp_hook_wrap_pri4(uintptr_t fp_addr, hook_chain4_callback before, hook_chain4_callback after, void *udata, int32_t priority)
{
    return fp_hook_wrap_pri(fp_addr, 4, (void *)before, (void *)after, udata, priority);
}

static inline hook_err_t fp_hook_wrap_pri5(uintptr_t fp_addr, hook_chain5_callback before, hook_chain5_callback after, void *udata, int32_t priority)
{
    return fp_hook_wrap_pri(fp_addr, 5, (void *)before, (void *)after, udata, priority);
}

static inline hook_err_t fp_hook_wrap_pri6(uintptr_t fp_addr, hook_chain6_callback before, hook_chain6_callback after, void *udata, int32_t priority)
{
    return fp_hook_wrap_pri(fp_addr, 6, (void *)before, (void *)after, udata, priority);
}

static inline hook_err_t fp_hook_wrap_pri7(uintptr_t fp_addr, hook_chain7_callback before, hook_chain7_callback after, void *udata, int32_t priority)
{
    return fp_hook_wrap_pri(fp_addr, 7, (void *)before, (void *)after, udata, priority);
}

static inline hook_err_t fp_hook_wrap_pri8(uintptr_t fp_addr, hook_chain8_callback before, hook_chain8_callback after, void *udata, int32_t priority)
{
    return fp_hook_wrap_pri(fp_addr, 8, (void *)before, (void *)after, udata, priority);
}

static inline hook_err_t fp_hook_wrap_pri9(uintptr_t fp_addr, hook_chain9_callback before, hook_chain9_callback after, void *udata, int32_t priority)
{
    return fp_hook_wrap_pri(fp_addr, 9, (void *)before, (void *)after, udata, priority);
}

static inline hook_err_t fp_hook_wrap_pri10(uintptr_t fp_addr, hook_chain10_callback before, hook_chain10_callback after, void *udata, int32_t priority)
{
    return fp_hook_wrap_pri(fp_addr, 10, (void *)before, (void *)after, udata, priority);
}

static inline hook_err_t fp_hook_wrap_pri11(uintptr_t fp_addr, hook_chain11_callback before, hook_chain11_callback after, void *udata, int32_t priority)
{
    return fp_hook_wrap_pri(fp_addr, 11, (void *)before, (void *)after, udata, priority);
}

static inline hook_err_t fp_hook_wrap_pri12(uintptr_t fp_addr, hook_chain12_callback before, hook_chain12_callback after, void *udata, int32_t priority)
{
    return fp_hook_wrap_pri(fp_addr, 12, (void *)before, (void *)after, udata, priority);
}

#endif /* _KP_HOOK_H_ */
