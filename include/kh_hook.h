/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2026 bmax121.
 *
 * Central public API: hook chain structs, error codes, and the
 * kh_hook_wrap / kh_fp_hook_wrap / kh_hook macros for all build modes.
 *
 * Build modes: shared
 * Depends on: types.h
 */

#ifndef _KP_HOOK_H_
#define _KP_HOOK_H_

#include <types.h>

typedef enum
{
    HOOK_NO_ERR = 0,
    HOOK_BAD_ADDRESS = 4095,
    HOOK_DUPLICATED = 4094,
    HOOK_NO_MEM = 4093,
    HOOK_BAD_RELO = 4092,
    HOOK_TRANSIT_NO_MEM = 4091,
    HOOK_CHAIN_FULL = 4090,
} kh_hook_err_t;

enum hook_type
{
    NONE = 0,
    INLINE,
    INLINE_CHAIN,
    FUNCTION_POINTER_CHAIN,
};

#define local_container_of(ptr, type, member) ({ (type *)((char *)(ptr) - offsetof(type, member)); })

#define HOOK_MEM_REGION_NUM 4
#define TRAMPOLINE_NUM 5
#define RELOCATE_INST_NUM (TRAMPOLINE_NUM * 6 + 6)

#define HOOK_CHAIN_NUM 0x08
#define TRANSIT_INST_NUM 0x24

#define FP_HOOK_CHAIN_NUM 0x10

#define ARM64_NOP 0xd503201f
#define ARM64_BTI_C 0xd503245f
#define ARM64_BTI_J 0xd503249f
#define ARM64_BTI_JC 0xd50324df
#define ARM64_PACIASP 0xd503233f
#define ARM64_PACIBSP 0xd503237f

/* ---- Shadow Call Stack (SCS) instructions ----
 * SCS protects return addresses by maintaining a separate shadow stack in X18.
 * SCS push/pop instructions appear in prologues/epilogues on ARM64 Linux kernels
 * compiled with CONFIG_SHADOW_CALL_STACK or -fsanitize=shadow-call-stack.
 *
 * These instructions are relocated normally via relo_ignore (NOT skipped or NOP'd).
 * SCS push/pop pairs balance naturally through the call chain:
 *   - Relocated prologue's SCS push pairs with the original epilogue's SCS pop
 *   - transit_body's own SCS push (if compiled with SCS) pairs with its own pop
 * Each level maintains its own balanced pair. No intervention needed.
 *
 * Detection purpose: diagnostics and compatibility verification — confirming that
 * TRAMPOLINE_MAX_NUM is large enough to cover BTI+PAC+SCS combinations.
 *
 * macOS/Apple Silicon does not use SCS, so these will never appear in macOS binaries.
 */
#define ARM64_SCS_PUSH 0xf800845e  /* str x30, [x18], #8  */
#define ARM64_SCS_POP  0xf85f8e5e  /* ldr x30, [x18, #-8]! */

/* ---- kCFI exemption ----
 * Kernel Control Flow Integrity (kCFI) validates indirect call targets by
 * comparing a hash embedded before the callee's entry point against the
 * expected type hash at the call site.  transit_body and fp_transit_body
 * perform indirect calls to relocated code (relo_addr) and user-registered
 * callbacks whose type signatures are intentionally heterogeneous — they
 * cannot carry valid kCFI hashes.
 *
 * Using __attribute__((no_sanitize("kcfi"))) on the body functions is
 * optimal over the alternative of embedding hash fields because:
 *   1. Relocated code is dynamically generated — there is no compile-time
 *      hash to embed.
 *   2. Callbacks are registered by the user with arbitrary signatures cast
 *      to a common type — the hash would need to match every possible
 *      callback signature, which is infeasible.
 *   3. The transit stubs are already in a non-standard code path (naked asm
 *      copied into ROX buffers), so kCFI instrumentation would not apply
 *      correctly regardless.
 */
#if __has_attribute(no_sanitize)
#define KCFI_EXEMPT __attribute__((no_sanitize("kcfi")))
#else
#define KCFI_EXEMPT
#endif

/* ---- PAC stripping ----
 * On PAC-enabled binaries, function pointers may carry signature bits in
 * the upper byte(s). Strip them at API entry so origin-map lookups and
 * address comparisons use the raw code address.
 *
 * Strip must preserve the sign-extension of bit 55: kernel VAs have bits
 * 63..56 == bit 55 == 1, user VAs have bits 63..56 == bit 55 == 0. A
 * naive `& ((1<<48)-1)` mask breaks kernel pointers by zeroing the
 * sign-extension, turning a valid kernel VA like 0xffffffc0_7a8ef034
 * into a garbage 0x0000ffc0_7a8ef034.
 *
 * When the compiler supports PAC intrinsics we prefer ptrauth_strip().
 * Otherwise we use an arithmetic shift (sign-extending bit 55) which
 * matches XPACI semantics for VA_BITS<=48 kernels. For non-PAC
 * freestanding builds there is no signing, so strip is identity. */
#if defined(__ARM_FEATURE_PAC_DEFAULT) && defined(KMOD_FREESTANDING)
#include <ptrauth.h>
#define STRIP_PAC(ptr) ((void *)ptrauth_strip((void *)(ptr), ptrauth_key_asia))
#elif defined(KMOD_FREESTANDING)
/* Non-PAC freestanding: no signing, identity preserves kernel VAs. */
#define STRIP_PAC(ptr) ((void *)(ptr))
#elif defined(__ARM_FEATURE_PAC_DEFAULT) && defined(__KERNEL__)
#define STRIP_PAC(ptr) ((void *)(((intptr_t)(ptr) << 8) >> 8))
#else
#define STRIP_PAC(ptr) ((void *)(ptr))
#endif

#define HOOK_LOCAL_DATA_NUM 4

/* ---- Core kh_hook_t (inline kh_hook state) ---- */

typedef struct
{
    /* in */
    uintptr_t func_addr;
    uintptr_t origin_addr;
    uintptr_t replace_addr;
    uintptr_t relo_addr;
    /* out */
    int32_t tramp_insts_num;
    int32_t relo_insts_num;
    uint32_t origin_insts[TRAMPOLINE_NUM] __aligned(8);
    uint32_t tramp_insts[TRAMPOLINE_NUM] __aligned(8);
    /* Relocated code with kCFI hash prefix.
     * _relo_cfi_hash = CFI type hash (copied from origin_addr - 4).
     * relo_insts[0..] = relocated instructions.
     * relo_addr points to &relo_insts[0], so *(relo_addr - 4) == _relo_cfi_hash.
     * On non-kCFI kernels _relo_cfi_hash is harmless unused data. */
    uint32_t _relo_cfi_hash;
    uint32_t relo_insts[RELOCATE_INST_NUM];
} kh_hook_t __aligned(8);

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
        };
        uint64_t data[HOOK_LOCAL_DATA_NUM];
    };
} kh_hook_local_t;

/* ---- Hook fargs: local is now a pointer ---- */

#define HOOK_FARGS_COMMON                                                                    \
    void *chain;                                                                             \
    int32_t skip_origin;                                                                         \
    kh_hook_local_t *local;                                                                     \
    uint64_t ret;

typedef struct
{
    HOOK_FARGS_COMMON
} kh_hook_fargs0_t __aligned(8);

typedef struct
{
    HOOK_FARGS_COMMON
    union
    {
        struct { uint64_t arg0; uint64_t arg1; uint64_t arg2; uint64_t arg3; };
        uint64_t args[4];
    };
} kh_hook_fargs4_t __aligned(8);

typedef kh_hook_fargs4_t kh_hook_fargs1_t;
typedef kh_hook_fargs4_t kh_hook_fargs2_t;
typedef kh_hook_fargs4_t kh_hook_fargs3_t;

typedef struct
{
    HOOK_FARGS_COMMON
    union
    {
        struct {
            uint64_t arg0; uint64_t arg1; uint64_t arg2; uint64_t arg3;
            uint64_t arg4; uint64_t arg5; uint64_t arg6; uint64_t arg7;
        };
        uint64_t args[8];
    };
} kh_hook_fargs8_t __aligned(8);

typedef kh_hook_fargs8_t kh_hook_fargs5_t;
typedef kh_hook_fargs8_t kh_hook_fargs6_t;
typedef kh_hook_fargs8_t kh_hook_fargs7_t;

typedef struct
{
    HOOK_FARGS_COMMON
    union
    {
        struct {
            uint64_t arg0; uint64_t arg1; uint64_t arg2; uint64_t arg3;
            uint64_t arg4; uint64_t arg5; uint64_t arg6; uint64_t arg7;
            uint64_t arg8; uint64_t arg9; uint64_t arg10; uint64_t arg11;
        };
        uint64_t args[12];
    };
} kh_hook_fargs12_t __aligned(8);

typedef kh_hook_fargs12_t kh_hook_fargs9_t;
typedef kh_hook_fargs12_t kh_hook_fargs10_t;
typedef kh_hook_fargs12_t kh_hook_fargs11_t;

/* ---- Callback typedefs (generated) ---- */

#define _HOOK_DEFINE_CB_TYPEDEF(N) \
    typedef void (*kh_hook_chain##N##_callback)(kh_hook_fargs##N##_t *fargs, void *udata);

_HOOK_DEFINE_CB_TYPEDEF(0)
_HOOK_DEFINE_CB_TYPEDEF(1)
_HOOK_DEFINE_CB_TYPEDEF(2)
_HOOK_DEFINE_CB_TYPEDEF(3)
_HOOK_DEFINE_CB_TYPEDEF(4)
_HOOK_DEFINE_CB_TYPEDEF(5)
_HOOK_DEFINE_CB_TYPEDEF(6)
_HOOK_DEFINE_CB_TYPEDEF(7)
_HOOK_DEFINE_CB_TYPEDEF(8)
_HOOK_DEFINE_CB_TYPEDEF(9)
_HOOK_DEFINE_CB_TYPEDEF(10)
_HOOK_DEFINE_CB_TYPEDEF(11)
_HOOK_DEFINE_CB_TYPEDEF(12)

/* ---- Per-chain-item data (AoS layout for cache locality) ---- */

typedef struct
{
    void *before;
    void *after;
    void *udata;
    int32_t priority;
    kh_hook_local_t local;
} kh_hook_chain_item_t __aligned(8);

/* ---- ROX/RW split: inline kh_hook chain ---- */

struct kh_hook_chain_rw;

typedef struct
{
    kh_hook_t kh_hook;
    struct kh_hook_chain_rw *rw;
    uint32_t transit[TRANSIT_INST_NUM];
} kh_hook_chain_rox_t __aligned(64);

typedef struct kh_hook_chain_rw
{
    kh_hook_chain_rox_t *rox;
    int32_t chain_items_max;
    int32_t argno;
    int32_t sorted_count;
    uint16_t occupied_mask;
    /* 2 bytes implicit padding */
    int32_t sorted_indices[HOOK_CHAIN_NUM];
    kh_hook_chain_item_t items[HOOK_CHAIN_NUM];
} kh_hook_chain_rw_t __aligned(8);

/* ---- Function pointer kh_hook ---- */

typedef struct
{
    uintptr_t fp_addr;
    uintptr_t replace_addr;
    uintptr_t origin_fp;
} kh_fp_hook_t __aligned(8);

/* ---- ROX/RW split: function pointer kh_hook chain ---- */

struct kh_fp_hook_chain_rw;

typedef struct
{
    kh_fp_hook_t kh_hook;
    struct kh_fp_hook_chain_rw *rw;
    uint32_t transit[TRANSIT_INST_NUM];
} kh_fp_hook_chain_rox_t __aligned(64);

typedef struct kh_fp_hook_chain_rw
{
    kh_fp_hook_chain_rox_t *rox;
    int32_t chain_items_max;
    int32_t argno;
    int32_t sorted_count;
    uint32_t occupied_mask;
    int32_t sorted_indices[FP_HOOK_CHAIN_NUM];
    kh_hook_chain_item_t items[FP_HOOK_CHAIN_NUM];
} kh_fp_hook_chain_rw_t __aligned(8);

/* ---- Utility ---- */

#ifdef __USERSPACE__
static inline int is_bad_address(void *addr)
{
    return addr == (void *)0;
}
#else
static inline int is_bad_address(void *addr)
{
    return ((uintptr_t)addr & 0x8000000000000000) != 0x8000000000000000;
}
#endif

/* ---- Hook prepare / install / uninstall ---- */

kh_hook_err_t kh_hook_prepare(kh_hook_t *kh_hook);
void kh_hook_install(kh_hook_t *kh_hook);
void kh_hook_uninstall(kh_hook_t *kh_hook);

/* ---- Inline kh_hook API ---- */

kh_hook_err_t kh_hook(void *func, void *replace, void **backup);
void kh_unhook(void *func);

kh_hook_err_t kh_hook_chain_add(kh_hook_chain_rw_t *rw, void *before, void *after, void *udata, int32_t priority);
void kh_hook_chain_remove(kh_hook_chain_rw_t *rw, void *before, void *after);

kh_hook_err_t kh_hook_wrap(void *func, int32_t argno, void *before, void *after, void *udata, int32_t priority);

void kh_hook_unwrap_remove(void *func, void *before, void *after, int remove);

static inline void kh_hook_unwrap(void *func, void *before, void *after)
{
    kh_hook_unwrap_remove(func, before, after, 1);
}

/* ---- Origin function access ---- */

static inline void *wrap_get_origin_func(void *hook_args)
{
    kh_hook_fargs0_t *args = (kh_hook_fargs0_t *)hook_args;
    kh_hook_chain_rox_t *rox = (kh_hook_chain_rox_t *)args->chain;
    return (void *)rox->kh_hook.relo_addr;
}

/* ---- Function pointer kh_hook API ---- */

void kh_fp_hook(uintptr_t fp_addr, void *replace, void **backup);
void kh_fp_unhook(uintptr_t fp_addr, void *backup);

kh_hook_err_t kh_fp_hook_wrap(uintptr_t fp_addr, int32_t argno, void *before, void *after, void *udata, int32_t priority);

void kh_fp_hook_unwrap(uintptr_t fp_addr, void *before, void *after);

static inline void *fp_get_origin_func(void *hook_args)
{
    kh_hook_fargs0_t *args = (kh_hook_fargs0_t *)hook_args;
    kh_fp_hook_chain_rox_t *rox = (kh_fp_hook_chain_rox_t *)args->chain;
    return (void *)rox->kh_hook.origin_fp;
}

/* ---- Transit buffer setup (userspace) ---- */

void kh_hook_chain_setup_transit(kh_hook_chain_rox_t *rox);
void kh_fp_hook_chain_setup_transit(kh_fp_hook_chain_rox_t *rox);

/* ---- Typed convenience wrappers (generated via X-macro) ---- */

#define _HOOK_WRAP_VARIANTS(N)                                                                     \
    static inline kh_hook_err_t kh_hook_wrap##N(void *func, kh_hook_chain##N##_callback before,             \
        kh_hook_chain##N##_callback after, void *udata) {                                             \
        return kh_hook_wrap(func, N, (void *)before, (void *)after, udata, 0);                        \
    }                                                                                              \
    static inline kh_hook_err_t kh_fp_hook_wrap##N(uintptr_t fp_addr, kh_hook_chain##N##_callback before,   \
        kh_hook_chain##N##_callback after, void *udata) {                                             \
        return kh_fp_hook_wrap(fp_addr, N, (void *)before, (void *)after, udata, 0);                  \
    }

_HOOK_WRAP_VARIANTS(0)
_HOOK_WRAP_VARIANTS(1)
_HOOK_WRAP_VARIANTS(2)
_HOOK_WRAP_VARIANTS(3)
_HOOK_WRAP_VARIANTS(4)
_HOOK_WRAP_VARIANTS(5)
_HOOK_WRAP_VARIANTS(6)
_HOOK_WRAP_VARIANTS(7)
_HOOK_WRAP_VARIANTS(8)
_HOOK_WRAP_VARIANTS(9)
_HOOK_WRAP_VARIANTS(10)
_HOOK_WRAP_VARIANTS(11)
_HOOK_WRAP_VARIANTS(12)

#endif /* _KP_HOOK_H_ */
