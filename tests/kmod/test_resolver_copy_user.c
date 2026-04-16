/* tests/kmod/test_resolver_copy_user.c */
/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <types.h>
#include <linux/uaccess.h>
#include "test_resolver_common.h"

/* Assert the copy_to_user and copy_from_user capabilities resolve to
 * non-NULL function pointers. We cannot easily validate end-to-end
 * copy semantics from module_init context without a live userspace
 * pointer — the existing uaccess tests in test_hook_kernel.c exercise
 * the full path once src/uaccess.c is rewired to the registry
 * (Task 21). */
int test_resolver_copy_user(void) {
    typedef unsigned long (*to_t)(void __user *, const void *, unsigned long);
    typedef unsigned long (*from_t)(void *, const void __user *, unsigned long);

    to_t   ftu = (to_t)0;
    from_t ffu = (from_t)0;

    int rc1 = kh_strategy_resolve("copy_to_user",   &ftu, sizeof(ftu));
    int rc2 = kh_strategy_resolve("copy_from_user", &ffu, sizeof(ffu));

    KH_TEST_ASSERT("copy_user", rc1 == 0, "copy_to_user unresolved");
    KH_TEST_ASSERT("copy_user", ftu != (to_t)0,
                   "copy_to_user resolved to NULL fn pointer");
    KH_TEST_ASSERT("copy_user", rc2 == 0, "copy_from_user unresolved");
    KH_TEST_ASSERT("copy_user", ffu != (from_t)0,
                   "copy_from_user resolved to NULL fn pointer");

    KH_TEST_PASS("copy_user");
    return 0;
}
