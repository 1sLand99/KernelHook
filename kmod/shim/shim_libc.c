/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Freestanding libc implementations for Mode A / SDK (Mode B) builds.
 *
 * Rationale: the freestanding shim used to declare these functions as
 * `extern` and rely on the kernel-exported symbols plus MODVERSIONS CRC
 * entries to satisfy them at module-load time. That path is fragile —
 * every GKI build has different CRCs; kmod_loader must hand-pick them
 * from a reference .ko. If the reference .ko doesn't cover a symbol the
 * loader falls back to sentinel CRCs (0xDEADBEXX), and the kernel
 * rejects the module with ENOENT.
 *
 * Rule of the house (see memory/feedback_ksyms_over_extern.md): replace
 * compile-time extern references to kernel symbols with runtime
 * ksyms_lookup, OR — for pure-algorithm libc like strcmp/memcmp — with
 * self-implementations here. Either way the kernelhook.ko's UND symbol
 * list stays empty and no `__versions` CRC lookup is required at load.
 *
 * This file is compiled into every freestanding .ko (kernelhook.ko,
 * kh_test.ko, examples). All functions are leaf, no kernel dependencies.
 */

#include <types.h>

/* ---- Memory primitives ----
 *
 * These MUST be externally-linked definitions (not static inline) because
 * clang's -ffreestanding still lowers __builtin_memcpy/memset/memmove
 * into real function calls for non-trivial sizes. The linker then looks
 * for a symbol named memcpy/memset/memmove and we satisfy it here.
 */

void *memset(void *s, int c, unsigned long n)
{
    unsigned char *p = (unsigned char *)s;
    unsigned char byte = (unsigned char)c;
    while (n--) *p++ = byte;
    return s;
}

void *memcpy(void *dst, const void *src, unsigned long n)
{
    unsigned char *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;
    while (n--) *d++ = *s++;
    return dst;
}

void *memmove(void *dst, const void *src, unsigned long n)
{
    unsigned char *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;
    if (d == s || n == 0) return dst;
    if (d < s) {
        while (n--) *d++ = *s++;
    } else {
        d += n;
        s += n;
        while (n--) *--d = *--s;
    }
    return dst;
}

int memcmp(const void *s1, const void *s2, unsigned long n)
{
    const unsigned char *a = (const unsigned char *)s1;
    const unsigned char *b = (const unsigned char *)s2;
    while (n--) {
        int d = (int)*a - (int)*b;
        if (d) return d;
        a++; b++;
    }
    return 0;
}

/* ---- String primitives ---- */

int strcmp(const char *a, const char *b)
{
    while (*a && *a == *b) { a++; b++; }
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

int strncmp(const char *a, const char *b, unsigned long n)
{
    while (n && *a && *a == *b) { a++; b++; n--; }
    if (n == 0) return 0;
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

char *strchr(const char *s, int c)
{
    char target = (char)c;
    for (;; s++) {
        if (*s == target) return (char *)s;
        if (*s == '\0') return (char *)0;
    }
}

/* strlcpy: copies at most size-1 bytes from src to dst, NUL-terminates
 * dst if size > 0, returns strlen(src) (so caller can detect truncation). */
unsigned long strlcpy(char *dst, const char *src, unsigned long size)
{
    const char *s = src;
    unsigned long n = size;

    if (n) {
        while (--n) {
            char c = *s++;
            *dst++ = c;
            if (!c) break;
        }
    }
    if (!n) {
        if (size) *dst = '\0';
        while (*s++) { /* count remaining source length */ }
    }
    return (unsigned long)(s - src - 1);
}
