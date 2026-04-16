/* tests/userspace/test_strategy_registry.c */
/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Userspace unit test: strategy registry priority ordering.
 *
 * Declares two stub strategies for capability "test_cap_a":
 *   stub_hi at priority 0 (succeeds, returns 0xDEADBEEF)
 *   stub_lo at priority 1 (always fails with -2)
 *
 * Verifies that kh_strategy_resolve() picks the higher-priority (lower
 * numeric) strategy first, and short-circuits without calling stub_lo.
 *
 * RED test: intentionally fails at link time because kh_strategy_init()
 * and kh_strategy_resolve() are not yet implemented (Task 3).
 */

#include "test_framework.h"
#include <kh_strategy.h>

static uint64_t g_stub_value;
static int g_stub_calls;

static int stub_resolve_fixed(void *out, size_t sz)
{
    g_stub_calls++;
    if (sz != sizeof(uint64_t))
        return -22;
    *(uint64_t *)out = g_stub_value;
    return 0;
}

static int stub_resolve_fail(void *out, size_t sz)
{
    (void)out;
    (void)sz;
    g_stub_calls++;
    return -2;
}

KH_STRATEGY_DECLARE(test_cap_a, stub_hi, 0, stub_resolve_fixed, sizeof(uint64_t));
KH_STRATEGY_DECLARE(test_cap_a, stub_lo, 1, stub_resolve_fail, sizeof(uint64_t));

TEST(priority_ordering)
{
    g_stub_value = 0xDEADBEEF;
    g_stub_calls = 0;
    uint64_t out = 0;

    ASSERT_EQ(0, kh_strategy_init());
    ASSERT_EQ(0, kh_strategy_resolve("test_cap_a", &out, sizeof(out)));
    ASSERT_EQ((uint64_t)0xDEADBEEF, out);
    /* Priority 0 succeeded, priority 1 must not have been called. */
    ASSERT_EQ(1, g_stub_calls);
}

int main(void)
{
    return RUN_ALL_TESTS();
}
