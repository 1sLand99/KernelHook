/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2026 bmax121.
 *
 * Ring 2 / Ring 3 test: minimal freestanding exporter module that validates
 * the kh_exports.S pipeline and cross-module symbol resolution at load time.
 *
 * Build modes: kernel
 * Depends on: kmod/src/export.c (kh_exports.S), symbol.h (ksyms_lookup),
 *   kmod/src/compat.c (kmod_compat_init)
 * Notes: No hooking performed — exists to validate ELF export sections
 *   (Ring 2) and that importer.ko resolves symbols against it (Ring 3).
 */

#include "shim.h"
#include <types.h>
#include <symbol.h>
#include "../../../kmod/src/compat.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("bmax121");
MODULE_DESCRIPTION("KernelHook export_link_test exporter");

MODULE_VERSIONS();
MODULE_VERMAGIC();
MODULE_THIS_MODULE();

/* Force into .data (not .bss) so kmod_loader's patch_elf_symbol can
 * write the resolved value to the file-backed section. BSS is NOBITS
 * and is zeroed by the kernel at module load time, defeating the patch. */
static unsigned long kallsyms_addr __attribute__((used, section(".data"))) = 0;
module_param(kallsyms_addr, ulong, 0444);
MODULE_PARM_DESC(kallsyms_addr, "Address of kallsyms_lookup_name (hex, required)");

static int __init exporter_init(void)
{
    int rc = kmod_compat_init(kallsyms_addr);
    if (rc) {
        pr_err("export_link_test exporter: compat init failed (%d)\n", rc);
        return rc;
    }
    pr_info("export_link_test exporter: loaded\n");
    return 0;
}

static void __exit exporter_exit(void)
{
    pr_info("export_link_test exporter: unloaded\n");
}

module_init(exporter_init);
module_exit(exporter_exit);
