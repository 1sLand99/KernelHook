/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Runtime kernel adaptation layer.
 * Resolves kallsyms_lookup_name, detects kernel version, initializes logging.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#ifdef CONFIG_KPROBES
#include <linux/kprobes.h>
#endif

#include <types.h>
#include <symbol.h>
#include <linux/printk.h>

int kmod_kernel_major = 0;
int kmod_kernel_minor = 0;
int kmod_kernel_patch = 0;

static int parse_kernel_version(const char *banner)
{
    const char *p = banner;
    int i;

    for (i = 0; i < 64 && *p; i++, p++) {
        if (*p >= '0' && *p <= '9')
            break;
    }
    if (!*p) return -1;

    kmod_kernel_major = 0;
    while (*p >= '0' && *p <= '9') {
        kmod_kernel_major = kmod_kernel_major * 10 + (*p - '0');
        p++;
    }
    if (*p != '.') return -1;
    p++;

    kmod_kernel_minor = 0;
    while (*p >= '0' && *p <= '9') {
        kmod_kernel_minor = kmod_kernel_minor * 10 + (*p - '0');
        p++;
    }
    if (*p != '.') return -1;
    p++;

    kmod_kernel_patch = 0;
    while (*p >= '0' && *p <= '9') {
        kmod_kernel_patch = kmod_kernel_patch * 10 + (*p - '0');
        p++;
    }

    return 0;
}

static int detect_kernel_version(void)
{
    uint64_t banner_addr = ksyms_lookup("linux_banner");
    if (!banner_addr) {
        pr_err("compat: failed to find linux_banner");
        return -1;
    }
    const char *banner = (const char *)banner_addr;
    if (parse_kernel_version(banner) != 0) {
        pr_err("compat: failed to parse kernel version from banner");
        return -1;
    }
    pr_info("compat: kernel version %d.%d.%d",
          kmod_kernel_major, kmod_kernel_minor, kmod_kernel_patch);
    return 0;
}

#if !defined(KMOD_FREESTANDING) && defined(CONFIG_KPROBES)
static unsigned long find_kallsyms_via_kprobes(void)
{
    struct kprobe kp = { .symbol_name = "kallsyms_lookup_name" };
    int ret = register_kprobe(&kp);
    if (ret < 0)
        return 0;
    unsigned long addr = (unsigned long)kp.addr;
    unregister_kprobe(&kp);
    return addr;
}
#endif

/* Defined in kmod/src/log.c */
extern int log_init(void);

int kmod_compat_init(unsigned long kallsyms_addr)
{
    if (kallsyms_addr) {
        ksyms_init(kallsyms_addr);
    }
#if !defined(KMOD_FREESTANDING) && defined(CONFIG_KPROBES)
    else {
        unsigned long addr = find_kallsyms_via_kprobes();
        if (!addr) {
            pr_err("kernelhook: failed to resolve kallsyms_lookup_name\n");
            return -1;
        }
        ksyms_init(addr);
    }
#else
    else {
        pr_err("kernelhook: kallsyms_addr module parameter required\n");
        return -1;
    }
#endif

    if (log_init() != 0) {
        pr_err("kernelhook: failed to initialize logging\n");
        return -1;
    }

    if (detect_kernel_version() != 0) {
        pr_warn("compat: kernel version detection failed, continuing anyway");
    }

    return 0;
}
