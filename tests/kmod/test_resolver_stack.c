/* tests/kmod/test_resolver_stack.c */
/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <types.h>
#include "test_resolver_common.h"

int test_resolver_stack(void) {
    uint64_t itu = 0, ts = 0;
    int rc_itu = kh_strategy_resolve("init_thread_union", &itu, sizeof(itu));
    int rc_ts  = kh_strategy_resolve("thread_size", &ts, sizeof(ts));

    KH_TEST_ASSERT("stack", rc_itu == 0 && itu != 0,
                   "init_thread_union unresolved");
    KH_TEST_ASSERT("stack", rc_ts == 0,
                   "thread_size unresolved");
    KH_TEST_ASSERT("stack",
                   ts == 8192 || ts == 16384 || ts == 32768,
                   "thread_size not in allowed set (8192/16384/32768)");
    KH_TEST_PASS("stack");
    return 0;
}
