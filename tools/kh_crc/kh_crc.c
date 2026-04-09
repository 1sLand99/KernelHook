/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * kh_crc — KernelHook freestanding EXPORT_SYMBOL CRC generator.
 *
 * Reads kmod/exports.manifest, emits one of:
 *   --mode=asm       → assembly populating __ksymtab_*\/__kcrctab_*
 *   --mode=header    → C header with KH_DECLARE_VERSIONS() macro
 *   --mode=symvers   → Module.symvers-compatible text
 *   --mode=self-test → run internal unit tests and exit
 *
 * Design: see docs/superpowers/specs/2026-04-09-freestanding-export-symbol-and-runtime-loader-design.md §6.3
 * Contracts 1-5: the CRC algorithm is FROZEN at v1. Do not change canonicalize()
 * or the CRC32 parameters without creating kh_crc_v2.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define KH_CRC_VERSION "1"

static void usage(const char *argv0)
{
    fprintf(stderr,
        "Usage: %s --mode=<asm|header|symvers|self-test> "
        "[--manifest=<path>] [--output=<path>]\n",
        argv0);
    exit(2);
}

int main(int argc, char **argv)
{
    (void)argc; (void)argv;
    usage(argv[0]);
    return 0;
}
