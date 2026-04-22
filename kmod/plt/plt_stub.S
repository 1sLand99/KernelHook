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

/*
 * Empty .altinstructions section so Android 15 GKI 6.6+ sees the header
 * it expects on every module.  Vendor modules always carry this section
 * (even if small); the kernel apply_alternatives() walks it at load
 * time, and pre-5.15 behaviour was to tolerate missing but some Android
 * common-kernel forks added a header-existence precondition.
 *
 * Content stays 0 bytes — no errata entries — so apply_alternatives
 * walks zero iterations and returns OK. Align 8 matches vendor.
 */
	.section .altinstructions, "a", %progbits
	.balign 8

/*
 * Android 15 GKI 6.6+ module loader checks this per-module `.note.Linux`
 * note and rejects the .ko (finit_module → -ENOEXEC, silent) if missing.
 * Vendor modules in /vendor/lib/modules carry the same 48-byte blob, so
 * we replicate it verbatim.  Layout (two notes back-to-back):
 *
 *   note 1: namesz=6, descsz=1,  type=0x100 ("OPEN"), name="Linux\0",
 *           desc=0x00 + 3B pad
 *   note 2: namesz=6, descsz=4,  type=0x101 ("func"), name="Linux\0",
 *           desc=0x00000000 + 0B pad
 *
 * (Type values and semantics are internal to Android common kernel; we
 * only need byte-equivalence to pass the pre-load check.)
 */
	.section .note.Linux, "a", %note
	.align 2
	/* note 1 */
	.long 6                    /* namesz */
	.long 1                    /* descsz */
	.long 0x00000100           /* type = NT_LINUX_OPEN */
	.asciz "Linux"             /* "Linux" + NUL = 6 bytes */
	.balign 4
	.byte 0                    /* descriptor byte */
	.balign 4
	/* note 2 */
	.long 6                    /* namesz */
	.long 4                    /* descsz */
	.long 0x00000101           /* type = NT_LINUX_func */
	.asciz "Linux"
	.balign 4
	.long 0                    /* descriptor */

/*
 * arm64 GNU property note — declares FEAT_BTI + FEAT_PAC to the kernel
 * module loader.  Matches clang's auto-emitted note under
 * -mbranch-protection=standard (the flag the C translation units use).
 * Android 15 GKI 6.6 and newer silently reject modules that don't
 * declare FEAT_PAC in .note.gnu.property (finit_module → -ENOEXEC,
 * no dmesg), because stock vendor modules all declare it.
 *
 * Layout per ELF64 GNU property note spec:
 *     uint32_t namesz   = 4            // "GNU\0"
 *     uint32_t descsz   = 16           // one property, aligned
 *     uint32_t type     = 5            // NT_GNU_PROPERTY_TYPE_0
 *     char     name[4]  = "GNU\0"
 *     -- property descriptor --
 *     uint32_t pr_type  = 0xc0000000   // GNU_PROPERTY_AARCH64_FEATURE_1_AND
 *     uint32_t pr_datasz= 4
 *     uint32_t pr_data  = 3            // FEAT_1_BTI (bit 0) | FEAT_1_PAC (bit 1)
 *     uint32_t pad      = 0            // align pr_data field to 8 bytes
 */
	.section .note.gnu.property, "a", %note
	.align 3
	.long 4                    /* n_namesz: sizeof("GNU") */
	.long 16                   /* n_descsz: sizeof(pr_type+pr_datasz+pr_data+pad) */
	.long 5                    /* n_type = NT_GNU_PROPERTY_TYPE_0 */
	.asciz "GNU"
	.long 0xc0000000           /* pr_type = GNU_PROPERTY_AARCH64_FEATURE_1_AND */
	.long 4                    /* pr_datasz */
	.long 3                    /* pr_data: FEAT_1_BTI | FEAT_1_PAC */
	.long 0                    /* padding */
