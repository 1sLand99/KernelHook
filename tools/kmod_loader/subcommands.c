/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * CLI subcommand implementations for kmod_loader.
 *
 * Only the four subcommands that are independent of the load path live
 * here: unload, list, devices, probe. load and info stay in kmod_loader.c
 * because they reference many static helpers in that file (patching,
 * ELF manipulation, legacy argv parser).
 */

#include "resolver.h"
#include "subcommands.h"
#include "devices_table.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <stdint.h>
#include <linux/unistd.h>

#ifndef __NR_delete_module
#define __NR_delete_module 106
#endif

/* Forward-declared from kmod_loader.c (existing helper). */
int parse_kver(int *major, int *minor);

/* ---- unload ---- */

int subcmd_unload(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: kmod_loader unload <name>\n");
        return 1;
    }
    if ((int)syscall(__NR_delete_module, argv[1], 0) != 0) {
        fprintf(stderr, "delete_module(%s): %s\n", argv[1], strerror(errno));
        return 1;
    }
    printf("Unloaded %s\n", argv[1]);
    return 0;
}

/* ---- list ---- */
/*
 * Enumerate /sys/module. This lists every currently-loaded kernel module,
 * KernelHook-related or not. No filtering for now — downstream scripts can
 * grep. Filtering by a kernelhook marker can be added later without
 * breaking the interface.
 */
int subcmd_list(int argc, char **argv)
{
    (void)argc; (void)argv;
    DIR *d = opendir("/sys/module");
    if (!d) {
        fprintf(stderr, "opendir(/sys/module): %s\n", strerror(errno));
        return 1;
    }
    struct dirent *de;
    int n = 0;
    while ((de = readdir(d)) != NULL) {
        if (de->d_name[0] == '.') continue;
        printf("%s\n", de->d_name);
        n++;
    }
    closedir(d);
    fprintf(stderr, "(%d modules)\n", n);
    return 0;
}

/* ---- devices ---- */
/*
 * Walk g_devices[] (codegenned from kmod/devices/ *.conf) and print a
 * table suitable for copy-paste into bug reports. Width-aligned columns.
 */
int subcmd_devices(int argc, char **argv)
{
    (void)argc; (void)argv;
    printf("%-28s  %-10s  %-8s  %s\n", "NAME", "KRELEASE", "VERIFIED", "DESCRIPTION");
    printf("%-28s  %-10s  %-8s  %s\n",
           "----------------------------",
           "----------", "--------", "-----------");
    int n = 0;
    for (const struct device_entry *e = g_devices; e->name; e++) {
        printf("%-28s  %-10s  %-8s  %s\n",
               e->name,
               e->match_kernelrelease,
               e->verified ? "yes" : "no",
               e->description ? e->description : "");
        n++;
    }
    printf("\n(%d device profiles)\n", n);
    return 0;
}

/* ---- probe ---- */
/*
 * Standalone probe-only run. Builds a minimal resolve_ctx_t with config
 * strategies DISABLED (no_config = 1) and runs resolve() for all values.
 * Dumps the resulting trace so the user can see what each probe strategy
 * returned without any config fallback.
 *
 * Useful for (a) debugging loader failures on new hardware and
 * (b) collecting data for a new kmod/devices/ *.conf contribution.
 */
int subcmd_probe(int argc, char **argv)
{
    (void)argc; (void)argv;
    resolve_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));

    parse_kver(&ctx.kmajor, &ctx.kminor);
    struct utsname u;
    if (uname(&u) == 0) {
        strncpy(ctx.uname_release, u.release, sizeof(ctx.uname_release) - 1);
    }

    /* Probe-only — don't pollute the trace with config matches. */
    ctx.no_config = 1;

    trace_entry_t trace[VAL__COUNT];
    int n = 0;
    for (int i = 0; i < VAL__COUNT; i++) {
        trace_entry_t t = {0};
        resolve((value_id_t)i, &ctx, &t);
        trace[n++] = t;
    }
    printf("kmod_loader probe: kernel %d.%d (%s)\n",
           ctx.kmajor, ctx.kminor, ctx.uname_release);
    trace_dump(trace, n);
    return 0;
}
