/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Error path coverage: exercise error codes and graceful handling of bad input. */

#include "test_framework.h"
#include <kh_hook.h>
#include <memory.h>
#include <hmem_user.h>

/* Target function — padded to >= 16 bytes for trampoline. */
__attribute__((noinline))
static int err_target(int a, int b)
{
    asm volatile("nop\n\tnop\n\tnop");
    return a + b;
}

static int (*volatile call_err_target)(int, int) __attribute__((unused)) = err_target;

/* Replacement for simple kh_hook tests */
__attribute__((noinline))
static int err_replace(int a, int b)
{
    asm volatile("nop\n\tnop\n\tnop");
    return a * b;
}

/* Second target for duplicate test */
__attribute__((noinline))
static int err_target2(int a, int b)
{
    asm volatile("nop\n\tnop\n\tnop");
    return a - b;
}

static void before_noop(kh_hook_fargs2_t *fargs, void *udata)
{
    (void)fargs; (void)udata;
}

/* ---- Tests ---- */

TEST(error_hook_null_func)
{
    int rc = kh_hmem_user_init();
    ASSERT_EQ(rc, 0);

    void *backup = NULL;
    kh_hook_err_t err = kh_hook(NULL, (void *)err_replace, &backup);
    ASSERT_EQ(err, HOOK_BAD_ADDRESS);

    kh_hmem_user_cleanup();
}

TEST(error_hook_null_replace)
{
    int rc = kh_hmem_user_init();
    ASSERT_EQ(rc, 0);

    void *backup = NULL;
    kh_hook_err_t err = kh_hook((void *)err_target, NULL, &backup);
    ASSERT_EQ(err, HOOK_BAD_ADDRESS);

    kh_hmem_user_cleanup();
}

TEST(error_hook_null_backup)
{
    int rc = kh_hmem_user_init();
    ASSERT_EQ(rc, 0);

    kh_hook_err_t err = kh_hook((void *)err_target, (void *)err_replace, NULL);
    ASSERT_EQ(err, HOOK_BAD_ADDRESS);

    kh_hmem_user_cleanup();
}

TEST(error_hook_duplicate)
{
    int rc = kh_hmem_user_init();
    ASSERT_EQ(rc, 0);

    void *backup1 = NULL;
    kh_hook_err_t err = kh_hook((void *)err_target2, (void *)err_replace, &backup1);
    ASSERT_EQ(err, HOOK_NO_ERR);

    void *backup2 = NULL;
    err = kh_hook((void *)err_target2, (void *)err_replace, &backup2);
    ASSERT_EQ(err, HOOK_DUPLICATED);

    kh_unhook((void *)err_target2);
    kh_hmem_user_cleanup();
}

TEST(error_wrap_null_func)
{
    int rc = kh_hmem_user_init();
    ASSERT_EQ(rc, 0);

    kh_hook_err_t err = kh_hook_wrap(NULL, 2, (void *)before_noop, NULL, NULL, 0);
    ASSERT_EQ(err, HOOK_BAD_ADDRESS);

    kh_hmem_user_cleanup();
}

TEST(error_fp_hook_null)
{
    int rc = kh_hmem_user_init();
    ASSERT_EQ(rc, 0);

    /* kh_fp_hook with addr=0 should return without crash */
    void *backup = NULL;
    kh_fp_hook(0, (void *)err_replace, &backup);
    /* No crash = pass */

    kh_hmem_user_cleanup();
}

TEST(error_unhook_null)
{
    int rc = kh_hmem_user_init();
    ASSERT_EQ(rc, 0);

    kh_unhook(NULL);
    /* No crash = pass */

    kh_hmem_user_cleanup();
}

TEST(error_unhook_unhooked)
{
    int rc = kh_hmem_user_init();
    ASSERT_EQ(rc, 0);

    kh_unhook((void *)err_target);
    /* No crash = pass */

    kh_hmem_user_cleanup();
}

TEST(error_double_unhook)
{
    int rc = kh_hmem_user_init();
    ASSERT_EQ(rc, 0);

    void *backup = NULL;
    kh_hook_err_t err = kh_hook((void *)err_target, (void *)err_replace, &backup);
    ASSERT_EQ(err, HOOK_NO_ERR);

    kh_unhook((void *)err_target);
    kh_unhook((void *)err_target);
    /* No crash = pass */

    kh_hmem_user_cleanup();
}

TEST(error_unwrap_while_empty)
{
    int rc = kh_hmem_user_init();
    ASSERT_EQ(rc, 0);

    /* Wrap then unwrap, then unwrap again on empty chain */
    kh_hook_err_t err = kh_hook_wrap((void *)err_target, 2,
                                    (void *)before_noop, NULL, NULL, 0);
    ASSERT_EQ(err, HOOK_NO_ERR);

    kh_hook_unwrap((void *)err_target, (void *)before_noop, NULL);

    /* Chain is now empty — unwrap a non-existent callback */
    kh_hook_unwrap((void *)err_target, (void *)before_noop, NULL);
    /* No crash = pass */

    kh_hmem_user_cleanup();
}

int main(void)
{
    return RUN_ALL_TESTS();
}
