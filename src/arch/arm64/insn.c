/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2023 bmax121. All Rights Reserved.
 */

#include <hook.h>
#include <insn.h>
#include <export.h>

static uint32_t can_b_rel(uint64_t src_addr, uint64_t dst_addr)
{
#define B_REL_RANGE ((1 << 25) << 2)
    return ((dst_addr >= src_addr) & (dst_addr - src_addr <= B_REL_RANGE)) ||
           ((src_addr >= dst_addr) & (src_addr - dst_addr <= B_REL_RANGE));
}

int32_t branch_relative(uint32_t *buf, uint64_t src_addr, uint64_t dst_addr)
{
    if (can_b_rel(src_addr, dst_addr)) {
        buf[0] = 0x14000000u | (((dst_addr - src_addr) & 0x0FFFFFFFu) >> 2u); // B <label>
        buf[1] = ARM64_NOP;
        return 2;
    }
    return 0;
}
KP_EXPORT_SYMBOL(branch_relative);

int32_t branch_absolute(uint32_t *buf, uint64_t addr)
{
    buf[0] = 0x58000051; // LDR X17, #8
    buf[1] = 0xd61f0220; // BR X17
    buf[2] = addr & 0xFFFFFFFF;
    buf[3] = addr >> 32u;
    return 4;
}
KP_EXPORT_SYMBOL(branch_absolute);

int32_t ret_absolute(uint32_t *buf, uint64_t addr)
{
    buf[0] = 0x58000051; // LDR X17, #8
    buf[1] = 0xD65F0220; // RET X17
    buf[2] = addr & 0xFFFFFFFF;
    buf[3] = addr >> 32u;
    return 4;
}
KP_EXPORT_SYMBOL(ret_absolute);

int32_t branch_from_to(uint32_t *tramp_buf, uint64_t src_addr, uint64_t dst_addr)
{
    (void)src_addr;
    return ret_absolute(tramp_buf, dst_addr);
}

#ifdef HOOK_INTO_BRANCH_FUNC

static uint64_t branch_func_addr_once(uint64_t addr)
{
    uint64_t ret = addr;
    uint32_t inst = *(uint32_t *)addr;
    if ((inst & MASK_B) == INST_B) {
        uint64_t imm26 = bits32(inst, 25, 0);
        uint64_t imm64 = sign64_extend(imm26 << 2u, 28u);
        ret = addr + imm64;
    } else if (inst == ARM64_BTI_C || inst == ARM64_BTI_J || inst == ARM64_BTI_JC) {
        ret = addr + 4;
    }
    return ret;
}

uint64_t branch_func_addr(uint64_t addr)
{
    uint64_t ret;
    for (;;) {
        ret = branch_func_addr_once(addr);
        if (ret == addr) break;
        addr = ret;
    }
    return ret;
}

#endif /* HOOK_INTO_BRANCH_FUNC */
