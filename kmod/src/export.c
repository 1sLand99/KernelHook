/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * KernelHook public API exports.
 *
 * This file is the human-readable source of truth for what is exported.
 * It MUST stay in sync with kmod/exports.manifest — enforced by
 * scripts/lint_exports.sh at build time.
 *
 * In freestanding mode (KMOD_FREESTANDING) KH_EXPORT is a no-op:
 *   the actual __ksymtab_xxx / __kcrctab_xxx sections are populated by
 *   kmod/generated/kh_exports.S (emitted by tools/kh_crc).
 *
 * In kbuild mode (Deliverable C, separate spec) KH_EXPORT resolves to
 *   the standard EXPORT_SYMBOL() macro.
 */

#include <kh_hook.h>
#include <memory.h>
#include <symbol.h>
#include <linux/export.h>

#define KH_EXPORT(sym) EXPORT_SYMBOL(sym)

KH_EXPORT(kh_hook_prepare);
KH_EXPORT(kh_hook_install);
KH_EXPORT(kh_hook_uninstall);

KH_EXPORT(kh_hook);
KH_EXPORT(kh_unhook);

KH_EXPORT(kh_hook_chain_add);
KH_EXPORT(kh_hook_chain_remove);
KH_EXPORT(kh_hook_wrap);
KH_EXPORT(kh_hook_unwrap_remove);
KH_EXPORT(kh_hook_chain_setup_transit);

KH_EXPORT(kh_fp_hook);
KH_EXPORT(kh_fp_unhook);
KH_EXPORT(kh_fp_hook_wrap);
KH_EXPORT(kh_fp_hook_unwrap);
KH_EXPORT(kh_fp_hook_chain_setup_transit);

KH_EXPORT(ksyms_lookup);
