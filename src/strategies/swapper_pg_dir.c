/* src/strategies/swapper_pg_dir.c */
/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * SP-7 Capability: swapper_pg_dir
 *
 * Four fallback strategies for resolving the kernel's top-level page
 * global directory (swapper_pg_dir) across GKI 4.4 -> 6.12:
 *   1. kallsyms       - direct lookup (works when CONFIG_KALLSYMS_ALL=y)
 *   2. init_mm_pgd    - scan init_mm for a kernel-VA page-aligned field
 *   3. ttbr1_walk     - read TTBR1_EL1 and translate PA -> VA via
 *                       memstart_addr + PAGE_OFFSET
 *   4. pg_end_anchor  - use swapper_pg_end - (PTRS_PER_PGD * 8)
 *
 * Build modes: freestanding + kbuild (not userspace -- kernel-only).
 */

#include <kh_strategy.h>
#include <kh_log.h>
#include <types.h>

#ifdef __USERSPACE__
/* swapper_pg_dir strategies are kernel-only (require ksyms + ttbr1_el1
 * access). In userspace builds the translation unit compiles to nothing. */
#else

#include <symbol.h>
#include <arch/arm64/pgtable.h>

static int strat_kallsyms(void *out, size_t sz)
{
    if (sz != sizeof(uint64_t)) return -22; /* -EINVAL */
    uint64_t v = ksyms_lookup("swapper_pg_dir");
    if (!v) return KH_STRAT_ENODATA;
    *(uint64_t *)out = v;
    return 0;
}

static int strat_init_mm_pgd(void *out, size_t sz)
{
    if (sz != sizeof(uint64_t)) return -22;
    uint64_t mm = ksyms_lookup("init_mm");
    if (!mm) return KH_STRAT_ENODATA;

    /* Scan first 0x100 bytes of init_mm for a kernel-VA, page-aligned value.
     * The pgd field exists somewhere in that range on every ARM64 GKI
     * variant. We accept the first plausible candidate. */
    uint64_t kva_min = kh_page_offset ? kh_page_offset : 0xffffff8000000000ULL;
    for (unsigned long off = 0; off < 0x100; off += 8) {
        uint64_t cand = *(uint64_t *)(mm + off);
        if (cand >= kva_min && cand != 0 && (cand & 0xFFF) == 0) {
            *(uint64_t *)out = cand;
            return 0;
        }
    }
    return KH_STRAT_ENODATA;
}

static int strat_ttbr1_walk(void *out, size_t sz)
{
    if (sz != sizeof(uint64_t)) return -22;

    uint64_t ttbr1;
    asm volatile("mrs %0, ttbr1_el1" : "=r"(ttbr1));
    /* TTBR1_EL1 layout (ARMv8-A 48-bit PA, 4K granule):
     *   bits[63:48]: ASID (16-bit) — must be masked off
     *   bits[47:12]: BADDR (page-aligned PA of PGD)
     *   bits[11:1]:  reserved
     *   bit[0]:      CnP
     * Correct mask: zero ASID (top 16) and in-page offset (bits[11:0]).
     * Prior `& ~0xFFFFULL` over-masked (zeroed bits[15:12] of valid PA when
     * PGD PA isn't on a 64K boundary — e.g. PA=0x80123000 got rounded to
     * 0x80120000, causing divergence from kallsyms swapper_pg_dir by 0x3000).
     * TODO LPA2 (52-bit PA): BADDR[51:48] live in TTBR1_EL1[5:2]; if we
     * ever target FEAT_LPA2 kernels, combine those bits in. */
    uint64_t pgd_pa = ttbr1 & 0x0000FFFFFFFFF000ULL;

    /* Recursive resolution: get memstart_addr via the registry. */
    uint64_t memstart = 0;
    int rc = kh_strategy_resolve("memstart_addr", &memstart, sizeof(memstart));
    if (rc) return rc; /* propagate ENODATA / EDEADLK */

    *(uint64_t *)out = pgd_pa - memstart + kh_page_offset;
    return 0;
}

static int strat_pg_end_anchor(void *out, size_t sz)
{
    if (sz != sizeof(uint64_t)) return -22;
    uint64_t end = ksyms_lookup("swapper_pg_end");
    if (!end) return KH_STRAT_ENODATA;

    /* swapper_pg_end sits just past the end of swapper_pg_dir. The pgd
     * spans PTRS_PER_PGD entries of 8 bytes. PTRS_PER_PGD = 1 << (kh_page_shift - 3). */
    uint64_t ptrs = 1ULL << (kh_page_shift - 3);
    *(uint64_t *)out = end - (ptrs * 8);
    return 0;
}

KH_STRATEGY_DECLARE(swapper_pg_dir, kallsyms,      0, strat_kallsyms,      sizeof(uint64_t));
/* init_mm_pgd: scans first 0x100 bytes of init_mm for a page-aligned kernel VA.
 * Best-effort heuristic — first plausible match wins; on some kernels the first
 * matching field may not be the pgd. */
KH_STRATEGY_DECLARE_FALLBACK(swapper_pg_dir, init_mm_pgd,  1, strat_init_mm_pgd,   sizeof(uint64_t));
/* ttbr1_walk: returns the LINEAR-MAP VA (pgd_pa + PAGE_OFFSET - memstart) while
 * kallsyms returns the KIMAGE-MAP VA. Same PA in both mappings, both usable
 * for pgtable walks, but byte-different. */
KH_STRATEGY_DECLARE_FALLBACK(swapper_pg_dir, ttbr1_walk,   2, strat_ttbr1_walk,   sizeof(uint64_t));
/* pg_end_anchor: arithmetic from swapper_pg_end. Assumes standard single-page
 * PGD; if a kernel uses a larger PGD layout, the computed anchor is wrong. */
KH_STRATEGY_DECLARE_FALLBACK(swapper_pg_dir, pg_end_anchor, 3, strat_pg_end_anchor, sizeof(uint64_t));

#endif /* !__USERSPACE__ */
