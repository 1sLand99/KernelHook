/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Fake <linux/module.h> for freestanding .ko builds.
 *
 * Provides MODULE_LICENSE, MODULE_AUTHOR, MODULE_DESCRIPTION,
 * module_init, module_exit, module_param — the subset needed by
 * KernelHook modules. Full implementation lives in shim.h; this
 * header just pulls it in.
 */

#ifndef _FAKE_LINUX_MODULE_H
#define _FAKE_LINUX_MODULE_H

#include "shim.h"

#endif /* _FAKE_LINUX_MODULE_H */
