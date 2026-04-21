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

# ---------- Dual-layout build ----------
# module-dual produces both ksymtab layouts so kmod_loader can pick the right
# variant at insmod time: prel32 for GKI 6.1+ (CONFIG_HAVE_ARCH_PREL32_RELOCATIONS=y)
# and abs64 for older GKI 5.4 / pre-5.4 kernels. Generated as
# kernelhook-prel32.ko and kernelhook-abs64.ko alongside kernelhook.ko.
# extable-format + kCFI-hash differences are patched live by kmod_loader; only
# the ksymtab layout ships as two binaries because prel32↔abs64 conversion is
# a full ELF rewrite (section-size + reloc-count change).
.PHONY: module-dual
module-dual:
	$(MAKE) module KH_KSYMTAB_LAYOUT=prel32
	@mv kernelhook.ko kernelhook-prel32.ko
	@rm -rf _kh_core _kh_kmod generated
	$(MAKE) module KH_KSYMTAB_LAYOUT=abs64
	@mv kernelhook.ko kernelhook-abs64.ko
	@echo "Built dual-layout: kernelhook-prel32.ko + kernelhook-abs64.ko"
