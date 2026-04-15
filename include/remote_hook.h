/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2026 bmax121.
 *
 * Remote process hook API: ptrace-based code injection into a target
 * process on Linux ARM64; stub returns -ENOTSUP on other platforms.
 *
 * Build modes: userspace
 * Depends on: types.h
 */

#ifndef _KP_REMOTE_HOOK_H_
#define _KP_REMOTE_HOOK_H_

#include <types.h>

#ifdef __USERSPACE__

/* Opaque handle for a remote kh_hook session. */
typedef struct kh_remote_hook_handle *kh_remote_hook_handle_t;

/*
 * Attach to a remote process for hooking.
 * Uses ptrace(PTRACE_ATTACH) on Linux; saves register state.
 * Returns handle on success, NULL on failure.
 */
kh_remote_hook_handle_t kh_remote_hook_attach(int pid);

/*
 * Detach from a remote process.
 * Restores saved register state and calls ptrace(PTRACE_DETACH).
 * Returns 0 on success, negative errno on failure.
 */
int kh_remote_hook_detach(kh_remote_hook_handle_t handle);

/*
 * Allocate memory in the remote process.
 * Injects an mmap syscall into the target by setting registers and single-stepping.
 * @handle: active remote kh_hook session
 * @size:   allocation size (page-aligned internally)
 * @prot:   protection flags (PROT_READ|PROT_WRITE, PROT_READ|PROT_EXEC, etc.)
 * Returns remote virtual address on success, 0 on failure.
 */
uint64_t kh_remote_hook_alloc(kh_remote_hook_handle_t handle, uint64_t size, int prot);

/*
 * Install kh_hook code in the remote process.
 * Writes transit_code to func_addr in the target process via process_vm_writev,
 * then injects mprotect + icache flush.
 * @handle:       active remote kh_hook session
 * @func_addr:    address of function to kh_hook in target process
 * @transit_code: kh_hook trampoline code to write
 * @transit_size: size of transit_code in bytes
 * Returns 0 on success, negative errno on failure.
 */
int kh_remote_hook_install(kh_remote_hook_handle_t handle, uint64_t func_addr,
                        const void *transit_code, uint64_t transit_size);

#endif /* __USERSPACE__ */
#endif /* _KP_REMOTE_HOOK_H_ */
