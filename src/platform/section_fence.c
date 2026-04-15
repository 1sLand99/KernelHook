/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2026 bmax121.
 *
 * Page-alignment sentinels defining __kh_text_fence_head/tail to isolate
 * library .text for permission-switching in the hook install path.
 *
 * Build modes: shared
 * Depends on: kh_page_align.h (KH_PAGE_ALIGN, platform linker macros)
 * Notes: On macOS (ld64) placed via -order_file; on GNU ld via linker
 *   script. Symbols are verified by userspace page-isolation tests.
 */

#include "kh_page_align.h"

__attribute__((used, aligned(KH_PAGE_ALIGN), noinline))
void __kh_text_fence_head(void)
{
    asm volatile("");
}

__attribute__((used, aligned(KH_PAGE_ALIGN), noinline))
void __kh_text_fence_tail(void)
{
    asm volatile("");
}
