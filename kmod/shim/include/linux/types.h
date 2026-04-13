/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Fake <linux/types.h> for freestanding .ko builds.
 *
 * In kbuild mode this file is never on the include path — the real
 * kernel header is used instead. types.h already handles the
 * freestanding/kbuild split, so we just forward to it.
 */

#ifndef _FAKE_LINUX_TYPES_H
#define _FAKE_LINUX_TYPES_H

#include <types.h>

#endif /* _FAKE_LINUX_TYPES_H */
