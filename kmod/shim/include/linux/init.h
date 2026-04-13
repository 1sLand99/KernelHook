/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Fake <linux/init.h> for freestanding .ko builds.
 * Provides __init / __exit section attributes.
 */

#ifndef _FAKE_LINUX_INIT_H
#define _FAKE_LINUX_INIT_H

#include <types.h>  /* __section */

#ifndef __init
#define __init __section(".init.text")
#endif

#ifndef __exit
#define __exit __section(".exit.text")
#endif

#endif /* _FAKE_LINUX_INIT_H */
