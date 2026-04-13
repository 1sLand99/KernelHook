/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2026 bmax121.
 *
 * Remote process hooking API — ptrace-based injection for Linux ARM64.
 * macOS returns -ENOTSUP for all operations.
 */

#ifndef _KP_REMOTE_HOOK_H_
#define _KP_REMOTE_HOOK_H_

#include <types.h>

#ifdef __USERSPACE__

/* Opaque handle for a remote hook session. */
typedef struct remote_hook_handle *remote_hook_handle_t;

/*
 * Attach to a remote process for hooking.
 * Uses ptrace(PTRACE_ATTACH) on Linux; saves register state.
 * Returns handle on success, NULL on failure.
 */
remote_hook_handle_t remote_hook_attach(int pid);

/*
 * Detach from a remote process.
 * Restores saved register state and calls ptrace(PTRACE_DETACH).
 * Returns 0 on success, negative errno on failure.
 */
int remote_hook_detach(remote_hook_handle_t handle);

/*
 * Allocate memory in the remote process.
 * Injects an mmap syscall into the target by setting registers and single-stepping.
 * @handle: active remote hook session
 * @size:   allocation size (page-aligned internally)
 * @prot:   protection flags (PROT_READ|PROT_WRITE, PROT_READ|PROT_EXEC, etc.)
 * Returns remote virtual address on success, 0 on failure.
 */
uint64_t remote_hook_alloc(remote_hook_handle_t handle, uint64_t size, int prot);

/*
 * Install hook code in the remote process.
 * Writes transit_code to func_addr in the target process via process_vm_writev,
 * then injects mprotect + icache flush.
 * @handle:       active remote hook session
 * @func_addr:    address of function to hook in target process
 * @transit_code: hook trampoline code to write
 * @transit_size: size of transit_code in bytes
 * Returns 0 on success, negative errno on failure.
 */
int remote_hook_install(remote_hook_handle_t handle, uint64_t func_addr,
                        const void *transit_code, uint64_t transit_size);

#endif /* __USERSPACE__ */
#endif /* _KP_REMOTE_HOOK_H_ */
