/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2026 bmax121.
 */

#ifdef __linux__

#include <platform.h>
#include <sys/mman.h>
#include <unistd.h>

uint64_t platform_page_size(void)
{
    static uint64_t cached;
    if (!cached)
        cached = (uint64_t)sysconf(_SC_PAGE_SIZE);
    return cached;
}

void *platform_alloc_rox(uint64_t size)
{
    void *p = mmap(NULL, size, PROT_READ | PROT_EXEC,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    return (p == MAP_FAILED) ? NULL : p;
}

void *platform_alloc_rw(uint64_t size)
{
    void *p = mmap(NULL, size, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    return (p == MAP_FAILED) ? NULL : p;
}

void platform_free(void *ptr, uint64_t size)
{
    if (ptr)
        munmap(ptr, size);
}

int platform_set_rw(uint64_t addr, uint64_t size)
{
    return mprotect((void *)addr, size, PROT_READ | PROT_WRITE);
}

int platform_set_ro(uint64_t addr, uint64_t size)
{
    return mprotect((void *)addr, size, PROT_READ);
}

int platform_set_rx(uint64_t addr, uint64_t size)
{
    return mprotect((void *)addr, size, PROT_READ | PROT_EXEC);
}

int platform_write_code(uint64_t addr, const void *data, uint64_t size)
{
    uint64_t ps = platform_page_size();
    uint64_t start = addr & ~(ps - 1);
    uint64_t end = (addr + size - 1) & ~(ps - 1);
    uint64_t prot_size = (end - start) + ps;

    if (mprotect((void *)start, prot_size, PROT_READ | PROT_WRITE) != 0)
        return -1;

    __builtin_memcpy((void *)addr, data, size);
    __builtin___clear_cache((char *)addr, (char *)(addr + size));

    if (mprotect((void *)start, prot_size, PROT_READ | PROT_EXEC) != 0)
        return -1;

    return 0;
}

void platform_flush_icache(uint64_t addr, uint64_t size)
{
    __builtin___clear_cache((char *)addr, (char *)(addr + size));
}

#endif /* __linux__ */
