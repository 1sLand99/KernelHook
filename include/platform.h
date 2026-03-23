/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2026 bmax121.
 */

#ifndef _KP_PLATFORM_H_
#define _KP_PLATFORM_H_

#include <ktypes.h>

/* ---- Platform detection ---- */

#if defined(__linux__)
#define KH_PLATFORM_LINUX 1
#elif defined(__APPLE__)
#define KH_PLATFORM_DARWIN 1
#else
#error "Unsupported platform: requires Linux or macOS"
#endif

#if !defined(__aarch64__)
#error "Unsupported architecture: requires ARM64"
#endif

/* ---- Platform API ---- */

/* Returns the system page size in bytes. */
uint64_t platform_page_size(void);

/* Allocate page-aligned memory with PROT_READ|PROT_EXEC (never RWX).
 * Returns NULL on failure. */
void *platform_alloc_rox(uint64_t size);

/* Allocate page-aligned memory with PROT_READ|PROT_WRITE (no execute).
 * Returns NULL on failure. */
void *platform_alloc_rw(uint64_t size);

/* Free memory previously allocated by platform_alloc_rox/rw. */
void platform_free(void *ptr, uint64_t size);

/* Change memory protection to read-write. Returns 0 on success. */
int platform_set_rw(uint64_t addr, uint64_t size);

/* Change memory protection to read-only. Returns 0 on success. */
int platform_set_ro(uint64_t addr, uint64_t size);

/* Change memory protection to read-execute. Returns 0 on success. */
int platform_set_rx(uint64_t addr, uint64_t size);

/* Flush instruction cache for the given range. */
void platform_flush_icache(uint64_t addr, uint64_t size);

#endif /* _KP_PLATFORM_H_ */
