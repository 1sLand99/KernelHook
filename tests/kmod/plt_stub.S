/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Empty PLT section stubs for freestanding .ko build.
 *
 * CONFIG_ARM64_MODULE_PLTS=y requires .plt and .init.plt sections.
 * The kernel module loader will populate them with PLT entries for
 * out-of-range relocations.  We provide empty sections here so
 * the section headers exist in the ELF.
 */
	.section .plt, "ax", %progbits
	.align 4
	nop

	.section .init.plt, "ax", %progbits
	.align 2
	nop
