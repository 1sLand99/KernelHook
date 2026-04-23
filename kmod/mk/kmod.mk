# SPDX-License-Identifier: GPL-2.0-or-later
# kmod.mk — includable build fragment for KernelHook freestanding .ko modules
#
# Usage (explicit):
#   MODULE_NAME   := my_hook
#   MODULE_SRCS   := my_hook.c
#   KERNELHOOK_DIR := /path/to/KernelHook/kmod
#   include $(KERNELHOOK_DIR)/mk/kmod.mk
#
# Usage (Kbuild-style shorthand):
#   obj-m := my_hook.o
#   KERNELHOOK := /path/to/KernelHook/kmod
#   include $(KERNELHOOK)/mk/kmod.mk

# ---------- Directory layout ----------

# KERNELHOOK_DIR: the kmod/ directory inside the KernelHook tree.
# Allow users to set KERNELHOOK as an alias.
KERNELHOOK_DIR ?= $(KERNELHOOK)
ifeq ($(KERNELHOOK_DIR),)
  # Derive from this file's location: kmod/mk/kmod.mk -> kmod/
  KERNELHOOK_DIR := $(patsubst %/mk/kmod.mk,%,$(lastword $(MAKEFILE_LIST)))
endif

# KH_ROOT: the KernelHook project root (parent of kmod/).
KH_ROOT := $(KERNELHOOK_DIR)/..

# ---------- kh_crc tool & generated files ----------

KH_CRC       := $(KH_ROOT)/tools/kh_crc/kh_crc
KH_MANIFEST  := $(KH_ROOT)/kmod/exports.manifest
KH_GEN_DIR   := $(KERNELHOOK_DIR)/generated
KH_EXPORTS_S := $(KH_GEN_DIR)/kh_exports.S
KH_SYMVERS_H := $(KERNELHOOK_DIR)/include/kernelhook/kh_symvers.h

# kh_crc is a HOST tool (reads the export manifest, emits .S/.h for the
# cross-compiled kmod). When the outer `make module` is invoked with
# CC=<NDK clang> the env var leaks into this sub-make and the tool gets
# built as an ARM64 binary that can't run on the host. Pin CC to host `cc`
# (kh_crc's own Makefile defaults CC ?= cc, so this just reinforces it
# against the parent override). Use HOSTCC if caller supplied one.
$(KH_CRC):
	$(MAKE) -C $(KH_ROOT)/tools/kh_crc CC="$(or $(HOSTCC),cc)"

$(KH_GEN_DIR):
	mkdir -p $@

# KH_KSYMTAB_LAYOUT selects the on-disk __ksymtab layout:
#   prel32 (default) — 12-byte struct with PREL32 relocs. Matches kernels
#                      built with CONFIG_HAVE_ARCH_PREL32_RELOCATIONS=y (GKI
#                      6.1+ on arm64, modern upstream).
#   abs64            — 24-byte struct with ABS64 pointers (value, name,
#                      namespace). Required for kernels 5.3..5.7 where
#                      HAVE_ARCH_PREL32_RELOCATIONS is OFF but namespaces
#                      are already defined (Android 11 GKI 5.4).
#   abs64_legacy     — 16-byte struct with ABS64 pointers (value, name
#                      only). Required for pre-5.3 kernels where namespace
#                      field does not exist in struct kernel_symbol
#                      (Android 9 GKI 4.4, Android 10 GKI 4.14).
# Mismatched layout causes strcmp crashes in find_symbol/cmp_name at load
# time because the kernel's pointer walk stride differs from our entry
# stride, landing `name` reads on stale/NULL fields.
KH_KSYMTAB_LAYOUT ?= prel32
ifeq ($(KH_KSYMTAB_LAYOUT),abs64)
  KH_CRC_ASM_MODE := asm-abs64
else ifeq ($(KH_KSYMTAB_LAYOUT),abs64_legacy)
  KH_CRC_ASM_MODE := asm-abs64-legacy
else
  KH_CRC_ASM_MODE := asm
endif

$(KH_EXPORTS_S): $(KH_CRC) $(KH_MANIFEST) | $(KH_GEN_DIR)
	$(KH_CRC) --mode=$(KH_CRC_ASM_MODE) --manifest=$(KH_MANIFEST) --output=$@

$(KH_SYMVERS_H): $(KH_CRC) $(KH_MANIFEST)
	$(KH_CRC) --mode=header --manifest=$(KH_MANIFEST) --output=$@

# Always lint export.c <-> manifest consistency before linking.
.PHONY: _kh_lint
_kh_lint:
	@$(KH_ROOT)/scripts/lint_manifest.sh

# ---------- Module name / sources ----------

# Auto-detect MODULE_NAME from obj-m if not set
ifndef MODULE_NAME
  ifdef obj-m
    MODULE_NAME := $(basename $(firstword $(obj-m)))
  else
    $(error MODULE_NAME or obj-m must be defined before including kmod.mk)
  endif
endif

MODULE_SRCS ?=

# ---------- Cross-compiler ----------
# Resolved by shared detector. Sets KH_CC / KH_LD / KH_AR / KH_CROSS_COMPILE.
# See kmod/mk/detect_toolchain.mk for the decision tree.
include $(KERNELHOOK_DIR)/mk/detect_toolchain.mk

CC := $(KH_CC)
LD := $(KH_LD)
CROSS_COMPILE := $(KH_CROSS_COMPILE)

# ---------- Kernel release / vermagic ----------

KERNELRELEASE ?= unknown
VERMAGIC ?= $(KERNELRELEASE) SMP preempt mod_unload modversions aarch64

# ---------- Compile flags ----------
#
# Plan 2 (runtime resolver) made MODULE_LAYOUT_CRC / PRINTK_CRC /
# THIS_MODULE_SIZE / MODULE_INIT_OFFSET / MODULE_EXIT_OFFSET overrides
# obsolete — those values are resolved at load time by kmod_loader's
# strategy chain and patched directly into the .ko buffer before
# init_module. The .ko compiles with placeholder sentinels defined
# in kmod/shim/shim.h (MODULE_VERSIONS macro).
#
# If a consumer still passes any of those variables via `make VAR=val`,
# the build silently ignores them — harmless, but they no longer affect
# the produced .ko.

# KH_CFI_MODE selects the control-flow integrity build mode:
#   kcfi  (default) — clang -fsanitize=kcfi, matches GKI 6.1+ arm64 kernels
#                     built with CONFIG_CFI_CLANG=y + kCFI (new scheme)
#   none            — no CFI sanitizer. Needed for kernels built with
#                     the older shadow CFI (CONFIG_CFI_CLANG=y on GKI 5.4
#                     arm64); those reject kCFI-compiled modules with
#                     "CFI failure (target: init_module)" panic in
#                     __cfi_slowpath at do_one_initcall time.
KH_CFI_MODE ?= kcfi
ifeq ($(KH_CFI_MODE),none)
  KH_CFI_CFLAGS :=
else
  KH_CFI_CFLAGS := -fsanitize=kcfi
endif

# KH_PAYLOAD=1 builds a self-contained ET_REL blob (kh_payload.o) meant to be
# grafted into a vendor .ko by tools/kmod_loader/graft_vendor_ko.  Compared to
# the standard `.ko` build:
#   - plt_stub.S (notes, .altinstructions, .text.ftrace_trampoline, .plt) is
#     NOT linked in — the host vendor .ko already carries the sections that
#     the kernel's module pre-init checks look for.
#   - kh_exports.S (__ksymtab via tools/kh_crc) is NOT linked in — consumers
#     of EXPORT_SYMBOL go through a full kernelhook.ko deployment; graft mode
#     is only the bootstrap payload.
#   - MODULE_{LICENSE,AUTHOR,DESCRIPTION,VERSIONS,VERMAGIC,PARM_DESC} and the
#     THIS_MODULE / __ksymtab / __versions / __param / .modinfo /
#     .gnu.linkonce.this_module sections are all provided by the host.
#   - Payload exports `kh_entry` (alias of kernelhook_init) and `kh_exit`
#     (alias of kernelhook_exit) as external symbols so the graft tool can
#     rewrite the host's .rela.gnu.linkonce.this_module init/exit relocs to
#     point at them.
KH_PAYLOAD ?= 0

# IMPORTANT: Do NOT add -fsanitize=shadow-call-stack to freestanding builds.
# The .ko must run on kernels with and without SCS. Our transit_body must
# not use x18 (SCS register), which may be uninitialized on non-SCS kernels.
# Target function SCS instructions are relocated verbatim and work correctly
# because x18 is managed by the target kernel's SCS infrastructure.
KH_CFLAGS := -DKMOD_FREESTANDING \
             -DVERMAGIC_STRING='"$(VERMAGIC)"' \
             -DMODULE_NAME='"$(MODULE_NAME)"' \
             -ffreestanding -fno-builtin -fno-stack-protector -fno-common \
             -fno-PIE -fno-pic \
             -mcmodel=large \
             -fno-optimize-sibling-calls \
             -mbranch-protection=standard \
             -I$(KERNELHOOK_DIR)/shim/include \
             -I$(KH_ROOT)/include \
             -I$(KH_ROOT)/include/arch/arm64 \
             -I$(KERNELHOOK_DIR)/shim \
             -I$(KERNELHOOK_DIR)/include \
             -march=armv8.5-a -O2 -Wall -Wextra -Werror \
             -Wno-unused-parameter \
             -Wno-unused-function \
             -Wno-unknown-sanitizers \
             $(KH_CFI_CFLAGS)

ifeq ($(KH_PAYLOAD),1)
  KH_CFLAGS += -DKH_PAYLOAD
endif
# -fno-optimize-sibling-calls + -mbranch-protection=standard: Pixel production
# kernels mark both kernel .text and module vmalloc .text with PROT_BTI, and
# Android 15 GKI 6.6 additionally requires modules to declare FEAT_PAC via
# .note.gnu.property (vendor modules compiled with pac-ret show `paciasp` at
# function entry; kernels without the note silently ENOEXEC).  `standard`
# = bti + pac-ret + pauth-lr, matching stock Android common kernel
# toolchain.  `paciasp` is a HINT instruction: on CPUs without FEAT_PAuth
# it decodes as NOP, so no ARMv8.2 floor is imposed at runtime.
# That means:
#   (a) indirect calls from our module into kernel (ksyms-resolved fn
#       pointers) must be BLR — never tail-called via BR.
#       `-fno-optimize-sibling-calls` forbids clang to rewrite
#       `return fn(...)` as BR.
#   (b) the kernel's BLR into our init_module / cleanup_module / any
#       callback must land on a BTI_C (or BTI_JC) landing pad, AND carry
#       PAC prologue.  `-mbranch-protection=standard` makes clang prefix
#       every function entry with `paciasp` + `bti c`.  Without PAC
#       declaration Android 15 kernel rejects the module pre-init.
#       and raises a BTI exception — instant kernel panic on
#       CONFIG_CFI_PERMISSIVE=n kernels like stock Pixel 6.

# Allow user to append extra flags
KH_CFLAGS += $(EXTRA_CFLAGS)

# ---------- Linker script ----------

KH_LDS := $(KERNELHOOK_DIR)/lds/kmod.lds

# ---------- Source files ----------

# Core library sources from $(KH_ROOT)/src/
_KH_CORE_SRCS := $(KH_ROOT)/src/hook.c \
                 $(KH_ROOT)/src/memory.c \
                 $(KH_ROOT)/src/symbol.c \
                 $(KH_ROOT)/src/arch/arm64/inline.c \
                 $(KH_ROOT)/src/arch/arm64/transit.c \
                 $(KH_ROOT)/src/arch/arm64/insn.c \
                 $(KH_ROOT)/src/arch/arm64/pgtable.c \
                 $(KH_ROOT)/src/platform/syscall.c \
                 $(KH_ROOT)/src/uaccess.c \
                 $(KH_ROOT)/src/sync.c \
                 $(KH_ROOT)/src/kh_strategy.c \
                 $(KH_ROOT)/src/strategies/swapper_pg_dir.c \
                 $(KH_ROOT)/src/strategies/kimage_voffset.c \
                 $(KH_ROOT)/src/strategies/memstart_addr.c \
                 $(KH_ROOT)/src/strategies/cred_task.c \
                 $(KH_ROOT)/src/strategies/uaccess_copy.c \
                 $(KH_ROOT)/src/strategies/cross_cpu.c \
                 $(KH_ROOT)/src/strategies/runtime_sizes.c

# kmod SDK sources from $(KERNELHOOK_DIR)/src/
_KH_KMOD_SRCS := $(KERNELHOOK_DIR)/src/mem_ops.c \
                 $(KERNELHOOK_DIR)/src/log.c \
                 $(KERNELHOOK_DIR)/src/transit_setup.c \
                 $(KERNELHOOK_DIR)/src/compat.c \
                 $(KERNELHOOK_DIR)/src/export.c \
                 $(KERNELHOOK_DIR)/src/kh_strategy_boot.c

# Shim infrastructure sources: freestanding libc + ksyms wrappers for
# kernel-exported functions. See memory/feedback_ksyms_over_extern.md.
# These eliminate UND references to strcmp, memcpy, debugfs_*, copy_*,
# add_taint, kstrtol, snprintf, vsnprintf — so kernelhook.ko only needs
# a single __versions entry (module_layout) and no kmod_loader CRC
# patching for kernel utility functions.
_KH_SHIM_SRCS := $(KERNELHOOK_DIR)/shim/shim_libc.c \
                 $(KERNELHOOK_DIR)/shim/shim_ksyms.c

# PLT stub
_KH_PLT_SRCS := $(KERNELHOOK_DIR)/plt/plt_stub.S

# kh_crc-generated exports (assembly)
_KH_GEN_SRCS := $(KH_EXPORTS_S)
_KH_GEN_OBJS := $(KH_GEN_DIR)/kh_exports.kmod.o

# ---------- Object files (use .kmod.o suffix, in subdirs) ----------

_KH_CORE_OBJS := $(patsubst $(KH_ROOT)/%.c,_kh_core/%.kmod.o,$(_KH_CORE_SRCS))
_KH_KMOD_OBJS := $(patsubst $(KERNELHOOK_DIR)/%.c,_kh_kmod/%.kmod.o,$(_KH_KMOD_SRCS))
_KH_SHIM_OBJS := $(patsubst $(KERNELHOOK_DIR)/%.c,_kh_kmod/%.kmod.o,$(_KH_SHIM_SRCS))
_KH_PLT_OBJS  := $(patsubst $(KERNELHOOK_DIR)/%.S,_kh_kmod/%.kmod.o,$(_KH_PLT_SRCS))
_KH_MOD_OBJS  := $(patsubst %.c,%.kmod.o,$(MODULE_SRCS))

_KH_ALL_OBJS := $(_KH_MOD_OBJS) $(_KH_CORE_OBJS) $(_KH_KMOD_OBJS) $(_KH_SHIM_OBJS) $(_KH_PLT_OBJS) $(_KH_GEN_OBJS)

# Payload builds: drop plt_stub (host-provided notes/ftrace/altinstructions).
# kh_exports.kmod.o is kept — consumer modules (hello_hook etc.) resolve
# kh_hook_inline / kh_unhook / ... through the payload's __ksymtab.  The
# graft tool merges the payload's __ksymtab into the host's so the kernel's
# find_sec("__ksymtab") picks up our exports.
ifeq ($(KH_PAYLOAD),1)
  _KH_ALL_OBJS := $(filter-out $(_KH_PLT_OBJS),$(_KH_ALL_OBJS))
endif

# ---------- Targets ----------

.PHONY: module loader clean

module: $(MODULE_NAME).ko
	@echo "Built $(MODULE_NAME).ko successfully"
	@file $(MODULE_NAME).ko

# kh_payload.o — a single ET_REL .o meant to be grafted into a vendor .ko.
# No linker script: the graft tool is responsible for merging sections into
# the host's existing section layout.  No .rela.kh.this_module rename either
# (payload does not emit MODULE_THIS_MODULE, so the section does not exist).
# kh_symvers.h is still generated because some C sources optionally include
# it; kh_exports.kmod.o itself is not linked in (filtered out above).
#
# Invoked from the outer Makefile's `payload` wrapper, which forces
# KH_PAYLOAD=1.  This rule refuses to run without it — building a
# kh_payload.o out of non-KH_PAYLOAD .kmod.o objects would produce a blob
# missing the kh_entry/kh_exit aliases required by graft_vendor_ko.
kh_payload.o: _kh_lint $(KH_SYMVERS_H) $(_KH_ALL_OBJS)
	@if [ "$(KH_PAYLOAD)" != "1" ]; then \
	    echo "ERROR: kh_payload.o needs KH_PAYLOAD=1 — run 'make payload' instead" >&2; \
	    exit 1; \
	fi
	@# Pass the linker script so __start___kh_strategies / __stop___kh_strategies
	@# (and other anchor symbols in the script) get defined during partial link.
	@# Empty host-provided sections (.note.*, .altinstructions, .text.ftrace_trampoline,
	@# .plt, .init.plt, __ksymtab etc.) are emitted but stay empty — graft_vendor_ko
	@# appends them as .kh-prefixed no-op sections that the kernel module loader
	@# walks as zero-entry ranges.
	$(LD) -r -T $(KH_LDS) -o $@ $(_KH_ALL_OBJS)

loader: $(KERNELHOOK_DIR)/loader/kmod_loader.c
	$(CC) -static -O2 -o kmod_loader $<

# Detect llvm-objcopy (from NDK or PATH)
_KH_OBJCOPY := $(shell which $(CROSS_COMPILE)objcopy 2>/dev/null || which llvm-objcopy 2>/dev/null)

$(MODULE_NAME).ko: _kh_lint $(KH_SYMVERS_H) $(_KH_ALL_OBJS)
	$(LD) -r -T $(KH_LDS) -o $@.tmp $(_KH_ALL_OBJS)
	@# lld renames .kh.this_module output section to .gnu.linkonce.this_module
	@# via linker script, but keeps .rela.kh.this_module as the relocation name.
	@# Kernel expects .rela.gnu.linkonce.this_module — fix with objcopy.
ifneq ($(_KH_OBJCOPY),)
	$(_KH_OBJCOPY) --rename-section .rela.kh.this_module=.rela.gnu.linkonce.this_module $@.tmp $@
	@rm -f $@.tmp
else
	@mv $@.tmp $@
	@echo "WARNING: llvm-objcopy not found, .rela.kh.this_module not renamed"
endif

# Module's own sources
%.kmod.o: %.c
	$(CC) $(KH_CFLAGS) -c $< -o $@

# Core library objects
_kh_core/%.kmod.o: $(KH_ROOT)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(KH_CFLAGS) -c $< -o $@

# kmod SDK objects (C)
_kh_kmod/%.kmod.o: $(KERNELHOOK_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(KH_CFLAGS) -c $< -o $@

# kmod SDK objects (assembly)
_kh_kmod/%.kmod.o: $(KERNELHOOK_DIR)/%.S
	@mkdir -p $(dir $@)
	$(CC) $(KH_CFLAGS) -c $< -o $@

# kh_crc-generated assembly (kh_exports.S -> kh_exports.kmod.o)
$(KH_GEN_DIR)/kh_exports.kmod.o: $(KH_EXPORTS_S) | $(KH_GEN_DIR)
	$(CC) $(KH_CFLAGS) -c $< -o $@

clean:
	rm -f $(MODULE_NAME).ko $(MODULE_NAME).ko.tmp $(_KH_MOD_OBJS)
	rm -f kh_payload.o
	rm -rf _kh_core/ _kh_kmod/ $(KH_GEN_DIR)
	rm -f kmod_loader
