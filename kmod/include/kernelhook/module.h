/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * <kernelhook/module.h> — SDK umbrella for kernel-module metadata, init,
 * and logging primitives.
 *
 * SDK consumers (modules that depend on kernelhook.ko at runtime) include
 * this header once to get:
 *   MODULE_LICENSE / MODULE_AUTHOR / MODULE_DESCRIPTION
 *   module_init / module_exit / __init / __exit
 *   pr_info / pr_err / pr_warn
 *   MODULE_VERSIONS / MODULE_VERMAGIC / MODULE_THIS_MODULE
 *     (freestanding builds only; no-op in Kbuild builds where kernel
 *      headers don't define them and Kbuild fills the metadata itself)
 *
 * Works under both SDK build paths:
 *   - in-tree freestanding pipeline (kmod/mk/kmod_sdk.mk -> kmod.mk);
 *     the shim under kmod/shim/include/ provides <linux/XXX>.
 *   - real Kbuild (Mode C) with a full kernel source tree; real
 *     <linux/XXX> headers are in the include path.
 */
#ifndef _KERNELHOOK_MODULE_H_
#define _KERNELHOOK_MODULE_H_

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/printk.h>

#endif /* _KERNELHOOK_MODULE_H_ */
