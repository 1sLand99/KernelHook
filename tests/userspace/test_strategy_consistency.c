/* tests/userspace/test_strategy_consistency.c */
/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Userspace unit test: kh_strategy_run_consistency_check().
 *
 * Two capabilities are declared in this binary:
 *   ct_ok  — strategies a and b both write 0x42 (agree)
 *   ct_bad — strategy a writes 0x42, strategy b writes 0x43 (disagree)
 *
 * After kh_strategy_init(), consistency_check() should return 1 mismatch:
 *   ct_ok  -> 0 mismatches (all strategies agree)
 *   ct_bad -> 1 mismatch (diverging output detected)
 *   total  -> 1
 */

#include "test_framework.h"
#include <kh_strategy.h>

static int agreeing_a(void *o, size_t s)
{
    (void)s;
    *(uint64_t *)o = 0x42;
    return 0;
}

static int agreeing_b(void *o, size_t s)
{
    (void)s;
    *(uint64_t *)o = 0x42;
    return 0;
}

static int disagreeing(void *o, size_t s)
{
    (void)s;
    *(uint64_t *)o = 0x43;
    return 0;
}

KH_STRATEGY_DECLARE(ct_ok,  a, 0, agreeing_a,  sizeof(uint64_t));
KH_STRATEGY_DECLARE(ct_ok,  b, 1, agreeing_b,  sizeof(uint64_t));
KH_STRATEGY_DECLARE(ct_bad, a, 0, agreeing_a,  sizeof(uint64_t));
KH_STRATEGY_DECLARE(ct_bad, b, 1, disagreeing, sizeof(uint64_t));

TEST(consistency_agreement)
{
    ASSERT_EQ(0, kh_strategy_init());
    int mismatches = kh_strategy_run_consistency_check();
    /* ct_ok: both strategies write 0x42 -> agree -> 0 mismatches
     * ct_bad: strategy a writes 0x42, strategy b writes 0x43 -> disagree -> 1 mismatch
     * total: 1 */
    ASSERT_EQ(1, mismatches);
}

int main(void)
{
    return RUN_ALL_TESTS();
}
