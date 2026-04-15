/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2026 bmax121.
 *
 * Test module memory backend API: kmod_hook_mem_init/cleanup wire the
 * kernel-side ROX/RW allocators for use by the test harness.
 *
 * Build modes: kernel
 * Depends on: types.h
 */

#ifndef _MEM_OPS_H_
#define _MEM_OPS_H_

int kmod_hook_mem_init(void);
void kmod_hook_mem_cleanup(void);

#endif /* _MEM_OPS_H_ */
