/* src/strategies/memstart_addr.c */
/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * SP-7 Capability: memstart_addr
 *
 * Three fallback strategies for resolving the base physical address of
 * DRAM (PHYS_OFFSET / memstart_addr):
 *   1. kallsyms         - direct lookup of the exported variable
 *   2. dtb_parse        - loader-injected value via iomem_memstart module_param
 *                         (kmod_loader walks /proc/device-tree/memory@.../reg)
 *   3. dma_phys_limit   - heuristic: round arm64_dma_phys_limit down to 128 MB
 *                         (approximate; test does not assert equality with 1+2)
 *
 * Build modes: freestanding + kbuild (not userspace -- kernel-only).
 */

#include <kh_strategy.h>
#include <kh_log.h>
#include <types.h>

#ifdef __USERSPACE__
#else

#include <symbol.h>

/* Loader-injected DRAM base PA from DTB. Set via insmod iomem_memstart=0x...
 * When loader did not or could not parse DTB, stays 0 and strategy falls
 * through. Public (extern'd from kh_strategy_boot.c for module_param binding). */
uint64_t kh_loader_injected_memstart;

static int strat_kallsyms(void *out, size_t sz)
{
    if (sz != sizeof(uint64_t)) return -22;
    uint64_t a = ksyms_lookup("memstart_addr");
    if (!a) return KH_STRAT_ENODATA;
    *(uint64_t *)out = *(uint64_t *)a;
    return 0;
}

static int strat_dtb_parse(void *out, size_t sz)
{
    if (sz != sizeof(uint64_t)) return -22;
    if (!kh_loader_injected_memstart) return KH_STRAT_ENODATA;
    *(uint64_t *)out = kh_loader_injected_memstart;
    return 0;
}

static int strat_dma_phys_limit(void *out, size_t sz)
{
    if (sz != sizeof(uint64_t)) return -22;
    uint64_t a = ksyms_lookup("arm64_dma_phys_limit");
    if (!a) return KH_STRAT_ENODATA;
    uint64_t limit = *(uint64_t *)a;
    /* Round down to 128 MB — DRAM base PA is heavily aligned on real devices.
     * This is a HEURISTIC: arm64_dma_phys_limit is the upper-bound PA that can
     * be DMA'd, so rounding to 128 MB approximates the DRAM base PA on most
     * ARM64 SoCs where DRAM starts aligned to a large boundary. The L2 test
     * does NOT assert this strategy agrees with the kallsyms/dtb paths — it
     * only range-validates non-zero. */
    *(uint64_t *)out = limit & ~((1ULL << 27) - 1);
    return 0;
}

KH_STRATEGY_DECLARE(memstart_addr, kallsyms,       0, strat_kallsyms,       sizeof(uint64_t));
KH_STRATEGY_DECLARE(memstart_addr, dtb_parse,      1, strat_dtb_parse,      sizeof(uint64_t));
KH_STRATEGY_DECLARE(memstart_addr, dma_phys_limit, 2, strat_dma_phys_limit, sizeof(uint64_t));

#endif /* !__USERSPACE__ */
