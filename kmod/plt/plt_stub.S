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
 * .text.ftrace_trampoline — REQUIRED by arm64 module_init_ftrace_plt()
 * on kernels built with CONFIG_DYNAMIC_FTRACE=y (all Android GKI 6.1+).
 * When the section is missing, module_finalize() returns -ENOEXEC with
 * zero dmesg output (see arch/arm64/kernel/module.c module_init_ftrace_plt
 * in android15-6.6).  Kernel writes one `struct plt_entry` (16 bytes on
 * arm64, index FTRACE_PLT_IDX=0) into this section at insmod, so we
 * reserve enough room.  Align 4 matches arm64 instruction alignment.
 *
 * This was the silent rejection gate on Pixel_35 / Pixel_37 AVDs — the
 * kernel pre-validation passes, relocations apply, then module_finalize
 * fails this check before init_module() is ever invoked.  Dmesg is
 * empty because the check has no pr_err/pr_warn; only an errno=8
 * surfaces to userspace via finit_module.
 */
	.section .text.ftrace_trampoline, "ax", %progbits
	.balign 4
	.fill 16, 1, 0

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
 * arm64 GNU property note: declares FEAT_BTI only (pr_data=1).
 *
 * Why hand-rolled at all:
 *   `ld -r` (relocatable link, what kmod.mk uses to produce
 *   kernelhook.ko.tmp) does NOT auto-merge GNU property notes from C
 *   translation units into the output.  Empirically: removing this
 *   note while leaving every C TU's clang-auto-emitted BTI note in
 *   place caused the whole .note.gnu.property section to vanish from
 *   kernelhook.ko.  Keeping at least one explicitly-emitted note in
 *   an input object is what triggers the linker to write the section.
 *
 * Why pr_data=1 (BTI only) — and NOT pr_data=3 (BTI|PAC):
 *   GNU_PROPERTY_AARCH64_FEATURE_1_AND is, per ELF spec, AND-merged
 *   across all input objects at link time.  Clang under our current
 *   `-mbranch-protection=bti` (see kmod/mk/kmod.mk) emits its own
 *   note with pr_data=1.  Setting the FEAT_PAC bit here would
 *   silently get masked out by the AND-merge — the previous version
 *   of this file declared pr_data=3 and the resulting .ko ended up
 *   with pr_data=1 anyway.  Match clang's intent exactly so the
 *   declaration tracks the actual emitted instructions (no paciasp,
 *   no retaa, no autiasp under bti-only mode).
 *
 * If a future kernel starts gating module-load on FEAT_PAC: change
 * pr_data to 3 here AND switch kmod.mk to `-mbranch-protection=pac-ret`
 * with `-march=armv8-a` so clang emits the HINT-form
 * `paciasp+autiasp+ret` triple (verified on Pixel 6 6.1.99 via stock
 * vendor 8021q.ko).  Declaring PAC without emitting PAC instructions
 * is mismatched and rejected by some kernel verifiers.
 *
 * Layout per ELF64 GNU property note spec:
 *     uint32_t namesz   = 4            // "GNU\0"
 *     uint32_t descsz   = 16           // one property, aligned
 *     uint32_t type     = 5            // NT_GNU_PROPERTY_TYPE_0
 *     char     name[4]  = "GNU\0"
 *     uint32_t pr_type  = 0xc0000000   // GNU_PROPERTY_AARCH64_FEATURE_1_AND
 *     uint32_t pr_datasz= 4
 *     uint32_t pr_data  = 1            // FEAT_1_BTI (bit 0) only
 *     uint32_t pad      = 0            // align pr_data to 8 bytes
 */
	.section .note.gnu.property, "a", %note
	.align 3
	.long 4                    /* n_namesz: sizeof("GNU") */
	.long 16                   /* n_descsz: sizeof(pr_type+pr_datasz+pr_data+pad) */
	.long 5                    /* n_type = NT_GNU_PROPERTY_TYPE_0 */
	.asciz "GNU"
	.long 0xc0000000           /* pr_type = GNU_PROPERTY_AARCH64_FEATURE_1_AND */
	.long 4                    /* pr_datasz */
	.long 1                    /* pr_data: FEAT_1_BTI only */
	.long 0                    /* padding */
