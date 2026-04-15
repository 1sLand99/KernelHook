/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2026 bmax121.
 *
 * Userspace hook memory pool initialization: kh_hmem_user_init/cleanup
 * wires the ROX/RW allocators to mmap-backed platform memory.
 *
 * Build modes: shared
 * Depends on: memory.h (kh_mem_ops_t), platform.h (kh_platform_alloc_rox)
 */

#ifndef _KP_HMEM_USER_H_
#define _KP_HMEM_USER_H_

int kh_hmem_user_init(void);
void kh_hmem_user_cleanup(void);

#endif /* _KP_HMEM_USER_H_ */
