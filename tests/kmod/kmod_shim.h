/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Minimal kernel shims for freestanding .ko build (Approach B).
 *
 * Replaces <linux/module.h>, <linux/kernel.h>, etc. for builds
 * that don't have access to the kernel source tree.
 *
 * The module loader only needs:
 *   - .modinfo section entries (license, description, etc.)
 *   - init_module / cleanup_module symbols
 *   - Proper ELF relocatable format (ET_REL)
 */

#ifndef _KMOD_SHIM_H_
#define _KMOD_SHIM_H_

#include <ktypes.h>

/* ---- .modinfo section entries ---- */

#define __MODULE_INFO(tag, name, info)                                  \
    static const char __UNIQUE_ID(name)[]                               \
        __used __section(".modinfo") __aligned(1) = #tag "=" info

#define __UNIQUE_ID(prefix) __PASTE(__PASTE(__unique_, prefix), __COUNTER__)
#define __PASTE(a, b) a##b

#define MODULE_LICENSE(x)       __MODULE_INFO(license, license, x)
#define MODULE_AUTHOR(x)        __MODULE_INFO(author, author, x)
#define MODULE_DESCRIPTION(x)   __MODULE_INFO(description, description, x)
#define MODULE_PARM_DESC(parm, desc) __MODULE_INFO(parm, parm, #parm ":" desc)

/* ---- Module init/exit via aliases ---- */

#define module_init(fn) \
    int init_module(void) __attribute__((alias(#fn)));
#define module_exit(fn) \
    void cleanup_module(void) __attribute__((alias(#fn)));

/* ---- Module parameter (simplified) ---- */
#define module_param(name, type, perm) \
    __MODULE_INFO(parmtype, name##type, #name ":" #type)

/* ---- Kernel PAGE_SIZE ---- */
#ifndef PAGE_SIZE
#define PAGE_SIZE 4096UL
#endif

/* ---- pr_info / pr_err ---- */
extern int printk(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

#define KERN_INFO    "\001" "6"
#define KERN_ERR     "\001" "3"
#define KERN_WARNING "\001" "4"

#define pr_info(fmt, ...)  printk(KERN_INFO fmt, ##__VA_ARGS__)
#define pr_err(fmt, ...)   printk(KERN_ERR fmt, ##__VA_ARGS__)
#define pr_warn(fmt, ...)  printk(KERN_WARNING fmt, ##__VA_ARGS__)

/* ---- Minimal bool ---- */
#ifndef true
#define true 1
#endif
#ifndef false
#define false 0
#endif

/* ---- __init / __exit section attributes ---- */
#define __init __section(".init.text")
#define __exit __section(".exit.text")

#endif /* _KMOD_SHIM_H_ */
