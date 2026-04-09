/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Ring 2 test: minimal freestanding exporter.
 *
 * Built through the full kmod.mk pipeline, so the resulting .ko pulls in
 * kmod/src/export.c + the kh_crc-generated kh_exports.S. That populates the
 * __ksymtab / __ksymtab_strings / __kcrctab sections with the real entries
 * (hook_wrap, ksyms_lookup, ...) that Ring 2's verify_elf.sh checks.
 *
 * No hooking is done here — the module exists purely to validate the export
 * pipeline at the ELF level (Ring 2) and that it loads cleanly (Ring 3).
 */

#include "shim.h"
#include <ktypes.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("bmax121");
MODULE_DESCRIPTION("KernelHook Ring 2 exporter test");

MODULE_VERSIONS();
MODULE_VERMAGIC();
MODULE_THIS_MODULE();

static int __init exporter_init(void)
{
    pr_info("export_link_test exporter: loaded\n");
    return 0;
}

static void __exit exporter_exit(void)
{
    pr_info("export_link_test exporter: unloaded\n");
}

module_init(exporter_init);
module_exit(exporter_exit);
