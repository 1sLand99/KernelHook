/* tests/kmod/test_resolver_swapper_pg_dir.c */
/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <types.h>
#include "test_resolver_common.h"

int test_resolver_swapper_pg_dir(void) {
    uint64_t golden = 0;
    int rc = kh_strategy_resolve("swapper_pg_dir", &golden, sizeof(golden));
    KH_TEST_ASSERT("swapper_pg_dir", rc == 0, "no strategy succeeded");
    KH_TEST_ASSERT("swapper_pg_dir", golden != 0, "returned NULL");

    /* Only compare the non-fallback kallsyms strategy against golden.
     * init_mm_pgd / ttbr1_walk / pg_end_anchor are marked _FALLBACK:
     *  - init_mm_pgd scans init_mm — heuristic
     *  - ttbr1_walk returns linear-map VA, kallsyms returns kimage VA
     *  - pg_end_anchor is arithmetic with kernel-specific assumptions
     * All three yield functionally-correct pgds for walking, but are not
     * byte-equal to kallsyms's kimage VA. Sanity-check they produce
     * kernel VAs and non-zero. */
    const char *names[] = {"kallsyms"};
    int i;
    for (i = 0; i < (int)(sizeof(names)/sizeof(names[0])); i++) {
        uint64_t v = 0;
        kh_strategy_force("swapper_pg_dir", names[i]);
        rc = kh_strategy_resolve("swapper_pg_dir", &v, sizeof(v));
        if (rc == 0) {
            KH_TEST_ASSERT("swapper_pg_dir", v == golden,
                           "kallsyms strategy disagrees with natural winner");
        }
    }
    /* Sanity check for the fallback heuristics: must produce a non-zero
     * kernel VA if they succeed. */
    const char *fb_names[] = {"init_mm_pgd", "ttbr1_walk", "pg_end_anchor"};
    for (i = 0; i < (int)(sizeof(fb_names)/sizeof(fb_names[0])); i++) {
        uint64_t v = 0;
        kh_strategy_force("swapper_pg_dir", fb_names[i]);
        rc = kh_strategy_resolve("swapper_pg_dir", &v, sizeof(v));
        if (rc == 0) {
            KH_TEST_ASSERT("swapper_pg_dir", v != 0,
                           "fallback returned NULL");
            KH_TEST_ASSERT("swapper_pg_dir", v >= 0xffff000000000000ULL,
                           "fallback returned non-kernel VA");
        }
    }
    kh_strategy_force("swapper_pg_dir", NULL);
    KH_TEST_PASS("swapper_pg_dir");
    return 0;
}
