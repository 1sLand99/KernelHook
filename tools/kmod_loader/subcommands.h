/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * CLI subcommand dispatch for kmod_loader.
 *
 * Each subcmd_* function takes the argv starting at the subcommand name
 * (so argv[0] == "load" for `kmod_loader load ...`, argv[0] == "unload"
 * for `kmod_loader unload ...`, and so on). Legacy positional form
 * (`kmod_loader <module.ko> [params...]`) is dispatched to subcmd_load
 * with the full original argv.
 */
#ifndef _KH_SUBCOMMANDS_H_
#define _KH_SUBCOMMANDS_H_

int subcmd_load(int argc, char **argv);
int subcmd_unload(int argc, char **argv);
int subcmd_list(int argc, char **argv);
int subcmd_info(int argc, char **argv);
int subcmd_devices(int argc, char **argv);
int subcmd_probe(int argc, char **argv);

#endif /* _KH_SUBCOMMANDS_H_ */
