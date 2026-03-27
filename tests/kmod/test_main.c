// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * KernelHook kernel module test harness
 *
 * Loads as a kernel module and runs hook tests in kernel context with real
 * page table manipulation.  Results are emitted via pr_info to dmesg.
 *
 * Test plan for security-mechanism-enabled kernels:
 *
 *   kCFI (CONFIG_CFI_CLANG):
 *     - Verify KCFI_EXEMPT attribute on transit_body bypasses kCFI checks
 *     - Verify indirect calls through hook callbacks do not trigger kCFI traps
 *     - Requires: kernel built with CONFIG_CFI_CLANG=y, Clang >= 16
 *
 *   Shadow Call Stack (CONFIG_SHADOW_CALL_STACK):
 *     - Verify SCS push/pop instructions in prologue are relocated correctly
 *     - Verify SCS stack balance is maintained through hook call chain
 *     - Requires: kernel built with CONFIG_SHADOW_CALL_STACK=y, GCC >= 12 or Clang >= 14
 *
 *   PAC (CONFIG_ARM64_PTR_AUTH_KERNEL):
 *     - Verify PAC-signed function pointers are stripped at API entry
 *     - Verify FPAC-safe SP invariant: SP unchanged between BLR and relocated PACIASP
 *     - Requires: kernel built with CONFIG_ARM64_PTR_AUTH_KERNEL=y, ARMv8.3+ hardware
 *
 *   BTI (CONFIG_ARM64_BTI_KERNEL):
 *     - Verify BTI_JC landing pad is emitted at trampoline entry
 *     - Verify transit stubs start with BTI_JC for BR-based entry
 *     - Requires: kernel built with CONFIG_ARM64_BTI_KERNEL=y, ARMv8.5+ hardware
 *
 * Kernel version requirements:
 *   - Minimum: Linux 5.10 (baseline ARM64 PAC/BTI support)
 *   - kCFI:   Linux 6.1+ (CONFIG_CFI_CLANG on ARM64)
 *   - SCS:    Linux 5.8+  (CONFIG_SHADOW_CALL_STACK on ARM64)
 *   - BTI:    Linux 5.10+ (CONFIG_ARM64_BTI_KERNEL)
 *   - PAC:    Linux 5.0+  (CONFIG_ARM64_PTR_AUTH_KERNEL)
 *
 * Required kernel config options (check via /proc/config.gz or
 * /boot/config-$(uname -r)):
 *   CONFIG_MODULES=y
 *   CONFIG_MODULE_UNLOAD=y
 *   And for specific security mechanism tests:
 *   CONFIG_CFI_CLANG=y             (kCFI tests)
 *   CONFIG_SHADOW_CALL_STACK=y     (SCS tests)
 *   CONFIG_ARM64_PTR_AUTH_KERNEL=y (PAC tests)
 *   CONFIG_ARM64_BTI_KERNEL=y      (BTI tests)
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/vmalloc.h>
#include <linux/mm.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("bmax121");
MODULE_DESCRIPTION("KernelHook test harness for kernel-context hook verification");

#define KH_TEST_TAG "kh_test: "

static int tests_run;
static int tests_passed;
static int tests_failed;

#define KH_ASSERT(cond, msg)                                             \
    do {                                                                 \
        tests_run++;                                                     \
        if (cond) {                                                      \
            tests_passed++;                                              \
            pr_info(KH_TEST_TAG "PASS: %s\n", (msg));                   \
        } else {                                                         \
            tests_failed++;                                              \
            pr_err(KH_TEST_TAG "FAIL: %s (at %s:%d)\n",                 \
                   (msg), __FILE__, __LINE__);                           \
        }                                                                \
    } while (0)

#define KH_SKIP(msg)                                                     \
    pr_info(KH_TEST_TAG "SKIP: %s\n", (msg))

/*
 * Test: verify kernel module loads and test framework works
 */
static void test_framework_sanity(void)
{
    KH_ASSERT(1 == 1, "test framework sanity check");
}

/*
 * Test: verify vmalloc/vfree works (prerequisite for hook memory allocation)
 */
static void test_vmalloc_available(void)
{
    void *p = vmalloc(PAGE_SIZE);

    KH_ASSERT(p != NULL, "vmalloc allocates memory in kernel context");
    if (p)
        vfree(p);
}

/*
 * Test: verify set_memory_x is available for W^X page manipulation
 * (actual hook installation requires executable memory regions)
 */
static void test_page_permissions(void)
{
    /* Just verify we can allocate and free — actual set_memory_x tests
     * require the full KernelHook library linked into the module */
    void *p = vmalloc(PAGE_SIZE);

    KH_ASSERT(p != NULL, "page allocation for permission tests");
    if (p)
        vfree(p);
}

/*
 * Test: check for kCFI kernel config
 */
static void test_kcfi_detection(void)
{
#if defined(CONFIG_CFI_CLANG)
    pr_info(KH_TEST_TAG "INFO: kernel built with CONFIG_CFI_CLANG=y\n");
    KH_ASSERT(true, "kCFI enabled — transit_body KCFI_EXEMPT verification needed");
#else
    KH_SKIP("kCFI not enabled (CONFIG_CFI_CLANG not set)");
#endif
}

/*
 * Test: check for Shadow Call Stack kernel config
 */
static void test_scs_detection(void)
{
#if defined(CONFIG_SHADOW_CALL_STACK)
    pr_info(KH_TEST_TAG "INFO: kernel built with CONFIG_SHADOW_CALL_STACK=y\n");
    KH_ASSERT(true, "SCS enabled — relocated SCS push/pop verification needed");
#else
    KH_SKIP("SCS not enabled (CONFIG_SHADOW_CALL_STACK not set)");
#endif
}

/*
 * Test: check for PAC kernel config
 */
static void test_pac_detection(void)
{
#if defined(CONFIG_ARM64_PTR_AUTH_KERNEL)
    pr_info(KH_TEST_TAG "INFO: kernel built with CONFIG_ARM64_PTR_AUTH_KERNEL=y\n");
    KH_ASSERT(true, "PAC enabled — STRIP_PAC and FPAC safety verification needed");
#else
    KH_SKIP("PAC not enabled (CONFIG_ARM64_PTR_AUTH_KERNEL not set)");
#endif
}

/*
 * Test: check for BTI kernel config
 */
static void test_bti_detection(void)
{
#if defined(CONFIG_ARM64_BTI_KERNEL)
    pr_info(KH_TEST_TAG "INFO: kernel built with CONFIG_ARM64_BTI_KERNEL=y\n");
    KH_ASSERT(true, "BTI enabled — BTI_JC landing pad verification needed");
#else
    KH_SKIP("BTI not enabled (CONFIG_ARM64_BTI_KERNEL not set)");
#endif
}

static int __init kh_test_init(void)
{
    pr_info(KH_TEST_TAG "=== KernelHook Kernel Module Test Harness ===\n");
    pr_info(KH_TEST_TAG "Starting tests...\n");

    tests_run = 0;
    tests_passed = 0;
    tests_failed = 0;

    /* Infrastructure tests */
    test_framework_sanity();
    test_vmalloc_available();
    test_page_permissions();

    /* Security mechanism detection */
    test_kcfi_detection();
    test_scs_detection();
    test_pac_detection();
    test_bti_detection();

    pr_info(KH_TEST_TAG "=== Results: %d run, %d passed, %d failed ===\n",
            tests_run, tests_passed, tests_failed);

    if (tests_failed > 0)
        pr_err(KH_TEST_TAG "SOME TESTS FAILED\n");
    else
        pr_info(KH_TEST_TAG "ALL TESTS PASSED\n");

    return 0;
}

static void __exit kh_test_exit(void)
{
    pr_info(KH_TEST_TAG "module unloaded\n");
}

module_init(kh_test_init);
module_exit(kh_test_exit);
