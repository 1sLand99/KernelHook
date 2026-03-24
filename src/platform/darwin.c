/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2026 bmax121.
 *
 * macOS / Apple Silicon platform backend.
 *
 * Code page patching uses vm_remap + VM_FLAGS_OVERWRITE to atomically
 * replace code pages.  The ROX pool uses mprotect for permission changes.
 */

#ifdef __APPLE__

#include <platform.h>
#include <sys/mman.h>
#include <unistd.h>
#include <mach/mach.h>
#include <libkern/OSCacheControl.h>

uint64_t platform_page_size(void)
{
    static uint64_t cached;
    if (!cached)
        cached = (uint64_t)sysconf(_SC_PAGE_SIZE);
    return cached;
}

/* On Darwin, both ROX and RW start as RW; the caller transitions
 * ROX pages to R|X after setup via platform_set_rx(). */
static void *darwin_alloc(uint64_t size)
{
    void *p = mmap(NULL, size, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    return (p == MAP_FAILED) ? NULL : p;
}

void *platform_alloc_rox(uint64_t size) { return darwin_alloc(size); }
void *platform_alloc_rw(uint64_t size)  { return darwin_alloc(size); }

void platform_free(void *ptr, uint64_t size)
{
    if (ptr)
        munmap(ptr, size);
}

int platform_set_rw(uint64_t addr, uint64_t size)
{
    mach_port_t task = mach_task_self();

    /* VM_PROT_COPY creates a CoW writable copy of the page(s). */
    kern_return_t kr = vm_protect(task, (vm_address_t)addr, (vm_size_t)size,
                                   FALSE,
                                   VM_PROT_READ | VM_PROT_WRITE | VM_PROT_COPY);
    return kr == KERN_SUCCESS ? 0 : -1;
}

int platform_set_ro(uint64_t addr, uint64_t size)
{
    return mprotect((void *)addr, (size_t)size, PROT_READ);
}

int platform_set_rx(uint64_t addr, uint64_t size)
{
    return mprotect((void *)addr, (size_t)size, PROT_READ | PROT_EXEC);
}

int platform_write_code(uint64_t addr, const void *data, uint64_t size)
{
    uint64_t ps = platform_page_size();
    uint64_t start = addr & ~(ps - 1);
    uint64_t end = (addr + size - 1) & ~(ps - 1);
    uint64_t prot_size = (end - start) + ps;
    uint64_t offset = addr - start;

    mach_port_t task = mach_task_self();

    /* On Apple Silicon, __TEXT pages have max_prot = R|X (no write).
     * Strategy: copy page(s) to a writable scratch buffer, patch there,
     * make scratch R|X, then vm_remap it over the original mapping. */
    void *scratch = mmap(NULL, prot_size, PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (scratch == MAP_FAILED)
        return -1;

    __builtin_memcpy(scratch, (void *)start, prot_size);
    __builtin_memcpy((char *)scratch + offset, data, size);

    mprotect(scratch, prot_size, PROT_READ | PROT_EXEC);

    vm_address_t target_addr = (vm_address_t)start;
    vm_prot_t cur_prot, max_prot;
    kern_return_t kr = vm_remap(task, &target_addr, (vm_size_t)prot_size, 0,
                                 VM_FLAGS_FIXED | VM_FLAGS_OVERWRITE,
                                 task, (vm_address_t)scratch, FALSE,
                                 &cur_prot, &max_prot, VM_INHERIT_NONE);

    munmap(scratch, prot_size);
    if (kr != KERN_SUCCESS)
        return -1;

    sys_icache_invalidate((void *)addr, (size_t)size);
    return 0;
}

void platform_flush_icache(uint64_t addr, uint64_t size)
{
    sys_icache_invalidate((void *)addr, (size_t)size);
}

#endif /* __APPLE__ */
