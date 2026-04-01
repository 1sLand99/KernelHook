// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Minimal probe module for kmod_loader binary offset probing.
 * init returns -EINVAL so kmod_loader can detect "init was called".
 */
#include "kmod_shim.h"

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("kmod_loader init offset probe");
MODULE_VERSIONS();
MODULE_VERMAGIC();

static int __init probe_init(void) { return -22; /* -EINVAL */ }
static void __exit probe_exit(void) { }

module_init(probe_init);
module_exit(probe_exit);
MODULE_THIS_MODULE();
