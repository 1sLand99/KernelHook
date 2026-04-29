/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2026 bmax121.
 *
 * Ring 2 / Ring 3 test: minimal SDK-mode importer module that resolves
 * kh_hook_wrap + ksyms_lookup against a loaded kernelhook.ko at runtime.
 *
 * Build modes: kernel
 * Depends on: kernelhook/kh_symvers.h (frozen CRCs), kh_hook.h, symbol.h
 * Notes: Core library NOT linked in — symbols resolved by kernel module
 *   loader against kernelhook.ko. MODULE_VERSIONS() under KH_SDK_MODE
 *   auto-emits the __versions entries for every kh_* export.
 */

#include "shim.h"
#include <types.h>
#include <kernelhook/kh_symvers.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("bmax121");
MODULE_DESCRIPTION("KernelHook Ring 2 importer test");

/* Re-declare the two symbols we reference. Signatures match include/kh_hook.h /
 * include/symbol.h, but we avoid including them so this TU stays minimal. */
extern uint64_t ksyms_lookup(const char *name);
extern int kh_hook_wrap(void *func, int argno, void *before, void *after,
                     void *udata, int priority);

static int __init importer_init(void)
{
    /* Use vfs_open — present on all kernels from 4.x through 6.x, unlike
     * do_sys_openat2 which only exists on 5.6+. Test must pass on GKI 5.4
     * (Pixel_30) through 6.1+ (Pixel_34+). */
    uint64_t addr = ksyms_lookup("vfs_open");
    pr_info("export_link_test importer: vfs_open = 0x%llx\n",
            (unsigned long long)addr);
    /* Force an UND reference to kh_hook_wrap so the symbol survives linking.
     * We never actually call it — addr==0 path returns before reaching it. */
    if (addr == 0) {
        (void)kh_hook_wrap((void *)(uintptr_t)addr, 4, 0, 0, 0, 0);
        return -1;
    }
    return 0;
}

static void __exit importer_exit(void)
{
    pr_info("export_link_test importer: unloaded\n");
}

MODULE_VERSIONS();
MODULE_VERMAGIC();
MODULE_THIS_MODULE();

module_init(importer_init);
module_exit(importer_exit);
