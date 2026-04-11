/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Fake <asm/set_memory.h> for freestanding .ko builds.
 * Forwards to linux/set_memory.h (mirrors kernel's pre-5.8 layout).
 */

#ifndef _FAKE_ASM_SET_MEMORY_H
#define _FAKE_ASM_SET_MEMORY_H

#include <linux/set_memory.h>

#endif /* _FAKE_ASM_SET_MEMORY_H */
