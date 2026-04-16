/* tests/userspace/test_strategy_registry.c */
/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Userspace unit tests: strategy registry semantics.
 *
 * Test 1 — priority_ordering:
 *   Declares two stub strategies for capability "test_cap_a":
 *     stub_hi at priority 0 (succeeds, returns 0xDEADBEEF)
 *     stub_lo at priority 1 (always fails with -2)
 *   Verifies that kh_strategy_resolve() picks the higher-priority (lower
 *   numeric) strategy first, and short-circuits without calling stub_lo.
 *
 * Test 2 — cycle_detection:
 *   Declares two mutually-recursive strategies (test_cycle_a / test_cycle_b).
 *   Verifies that the registry returns KH_STRAT_ENODATA (not an infinite
 *   loop) when a cycle is detected via the in_flight guard.
 *
 * Test 3 — cache_hit:
 *   Declares a counting resolver for "test_cache".
 *   Verifies that a second resolve() of the same capability hits the cache
 *   without invoking the resolver again.
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

/* --- cycle detection test --- */

static int cycle_b_resolve(void *out, size_t sz)
{
    uint64_t tmp = 0;
    int rc = kh_strategy_resolve("test_cycle_a", &tmp, sizeof(tmp));
    if (rc != 0)
        return rc;
    *(uint64_t *)out = tmp + 1;
    return 0;
}

static int cycle_a_resolve(void *out, size_t sz)
{
    uint64_t tmp = 0;
    int rc = kh_strategy_resolve("test_cycle_b", &tmp, sizeof(tmp));
    if (rc != 0)
        return rc;
    *(uint64_t *)out = tmp + 1;
    return 0;
}

KH_STRATEGY_DECLARE(test_cycle_a, only, 0, cycle_a_resolve, sizeof(uint64_t));
KH_STRATEGY_DECLARE(test_cycle_b, only, 0, cycle_b_resolve, sizeof(uint64_t));

TEST(cycle_detection)
{
    uint64_t out = 0;
    int rc = kh_strategy_resolve("test_cycle_a", &out, sizeof(out));
    /* Mutual recursion: test_cycle_a -> test_cycle_b -> test_cycle_a (in_flight)
     * -> EDEADLK -> cycle_b_resolve returns EDEADLK -> outer resolve("test_cycle_b")
     * sees rc!=0, does not cache, collapses to ENODATA -> cycle_a_resolve returns
     * ENODATA -> outer resolve("test_cycle_a") collapses to ENODATA.
     * Both caps end in_flight=false, cached=false — no test pollution. */
    ASSERT_EQ(KH_STRAT_ENODATA, rc);
}

/* --- cache hit test --- */

static int g_cache_hit_calls;

static int cache_counted_resolve(void *out, size_t sz)
{
    g_cache_hit_calls++;
    *(uint64_t *)out = 0x1111;
    return 0;
}

KH_STRATEGY_DECLARE(test_cache, only, 0, cache_counted_resolve, sizeof(uint64_t));

TEST(cache_hit)
{
    uint64_t a = 0, b = 0;
    g_cache_hit_calls = 0;

    ASSERT_EQ(0, kh_strategy_resolve("test_cache", &a, sizeof(a)));
    ASSERT_EQ(0, kh_strategy_resolve("test_cache", &b, sizeof(b)));
    ASSERT_EQ((uint64_t)0x1111, a);
    ASSERT_EQ((uint64_t)0x1111, b);
    /* Resolver must have been invoked only once; second call is a cache hit. */
    ASSERT_EQ(1, g_cache_hit_calls);
}

int main(void)
{
    return RUN_ALL_TESTS();
}
