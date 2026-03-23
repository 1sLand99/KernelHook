/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2026 bmax121.
 * Userspace hook memory initialization.
 */

#ifndef _KP_HOOK_MEM_USER_H_
#define _KP_HOOK_MEM_USER_H_

/*
 * Initialize the hook memory subsystem with userspace platform backends.
 * Calls hook_mem_init() with ROX and RW ops built from platform.h functions.
 * Returns 0 on success.
 */
int hook_mem_user_init(void);

/*
 * Cleanup the hook memory subsystem.
 * Calls hook_mem_cleanup().
 */
void hook_mem_user_cleanup(void);

#endif /* _KP_HOOK_MEM_USER_H_ */
