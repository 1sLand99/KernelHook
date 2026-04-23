# SPDX-License-Identifier: GPL-2.0-or-later
# Build kernelhook.ko — the prebuilt KernelHook kernel module.
#
# Usage:
#   make module        — build kernelhook.ko (freestanding, no kernel headers)
#   make loader        — build kmod_loader helper
#   make clean         — remove build artifacts

MODULE_NAME  := kernelhook
MODULE_SRCS  := src/main.c
KERNELHOOK_DIR := $(shell pwd)

include mk/kmod.mk

# ---------- Multi-layout build ----------
# module-dual produces every ksymtab layout so kmod_loader can pick the right
# variant at insmod time:
#   - prel32       : GKI 6.1+ (CONFIG_HAVE_ARCH_PREL32_RELOCATIONS=y, 12 B)
#   - abs64        : GKI 5.4..5.15 with namespaces but no PREL32 (24 B)
#   - abs64_legacy : pre-5.3 kernels (Android 9 4.4, Android 10 4.14) where
#                    struct kernel_symbol has no namespace field (16 B)
# extable-format + kCFI-hash differences are patched live by kmod_loader; only
# the ksymtab layout ships as multiple binaries because the on-disk entry
# stride is encoded into the section (sh_size, reloc count) — a full ELF
# rewrite at load time would be considerably more complex than shipping the
# three prebuilt variants.
.PHONY: module-dual
module-dual:
	@# Build legacy first, then abs64, then prel32.  Each build wipes the
	@# previous kmod object tree to avoid stale kh_exports.kmod.o (which
	@# encodes the layout choice and must regenerate).  The final `module`
	@# run uses prel32 so the default kernelhook.ko is the prel32 variant.
	$(MAKE) module KH_KSYMTAB_LAYOUT=abs64_legacy
	@mv kernelhook.ko kernelhook-abs64-legacy.ko
	@rm -rf _kh_core _kh_kmod generated
	$(MAKE) module KH_KSYMTAB_LAYOUT=abs64
	@mv kernelhook.ko kernelhook-abs64.ko
	@rm -rf _kh_core _kh_kmod generated
	$(MAKE) module KH_KSYMTAB_LAYOUT=prel32
	@cp kernelhook.ko kernelhook-prel32.ko
	@echo "Built multi-layout: kernelhook-prel32.ko + kernelhook-abs64.ko + kernelhook-abs64-legacy.ko"

# ---------- Vendor-ko graft payload ----------
# Produces kh_payload.o — a self-contained ET_REL blob that tools/kmod_loader/
# graft_vendor_ko merges into a vendor .ko's ELF.  See mk/kmod.mk KH_PAYLOAD
# comment for the full rationale.  Existing .kmod.o files were compiled
# without -DKH_PAYLOAD and are missing the alias declarations, so wipe them
# first.  The recursive `make` reuses this Makefile with KH_PAYLOAD=1 set,
# which activates the payload branches in mk/kmod.mk.
#
# The outer target intentionally differs from the inner rule name (`payload`
# vs `kh_payload.o`): make treats them as distinct, avoiding infinite
# recursion from a self-referential .PHONY target.
.PHONY: payload
payload:
	@# src/main.kmod.o lives in the source tree (not under _kh_core/_kh_kmod)
	@# so it survives the subdir wipe.  Remove it explicitly, otherwise a
	@# previously built non-payload main.o is reused and kh_entry/kh_exit
	@# never appear in the final kh_payload.o.
	@rm -rf _kh_core _kh_kmod generated
	@rm -f src/main.kmod.o
	$(MAKE) kh_payload.o KH_PAYLOAD=1
