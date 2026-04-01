# SPDX-License-Identifier: GPL-2.0-or-later
# Convenience Makefile for KernelHook kernel module test harness
#
# Targets:
#   make              — Approach A: build via Kbuild (requires kernel headers)
#   make KDIR=/path   — Approach A: build against a specific kernel source tree
#   make freestanding — Approach B: build with cross-compiler only (no kernel headers)
#   make clean        — remove build artifacts
#   make test         — run via scripts/run_android_tests.sh --kmod

KDIR ?= /lib/modules/$(shell uname -r)/build
PWD  := $(shell pwd)

# KernelHook project root
KH_ROOT := $(PWD)/../..

# Cross-compiler for freestanding build
CROSS_COMPILE ?= aarch64-linux-gnu-
CC := $(CROSS_COMPILE)gcc
LD := $(CROSS_COMPILE)ld

# Kernel release string (pass from device: KERNELRELEASE=$(uname -r))
KERNELRELEASE ?= unknown

# Construct vermagic: "<uname -r> SMP preempt mod_unload modversions aarch64"
VERMAGIC ?= $(KERNELRELEASE) SMP preempt mod_unload modversions aarch64

# Freestanding build flags
FS_CFLAGS := -DKMOD_FREESTANDING \
             -DVERMAGIC_STRING='"$(VERMAGIC)"' \
             -ffreestanding -fno-builtin -fno-stack-protector -fno-common \
             -fno-PIE -fno-pic \
             -I$(KH_ROOT)/include -I$(KH_ROOT)/include/arch/arm64 \
             -I$(PWD) \
             -march=armv8.5-a -O2 -Wall -Wextra -Werror \
             -Wno-unused-parameter -Wno-unused-function

# kCFI: only for 6.1+ kernels (CONFIG_CFI_CLANG uses kCFI).
# 5.10/5.15 use shadow-based CFI — kCFI metadata causes panic.
KVER_MAJOR := $(word 1,$(subst ., ,$(KERNELRELEASE)))
KVER_MINOR := $(word 2,$(subst ., ,$(KERNELRELEASE)))
KCFI_OK := $(shell [ "$(KVER_MAJOR)" -gt 6 ] 2>/dev/null && echo y || \
             ([ "$(KVER_MAJOR)" -eq 6 ] && [ "$(KVER_MINOR)" -ge 1 ] 2>/dev/null && echo y || echo n))
ifeq ($(KCFI_OK),y)
FS_CFLAGS += -fsanitize=kcfi
endif

# struct module init/exit offsets differ by kernel version.
# Compile with target kernel's offsets so kmod_loader doesn't need to patch relocations.
# Default: GKI 6.1 offsets (from kmod_shim.h)
ifeq ($(KVER_MAJOR).$(KVER_MINOR),5.10)
FS_CFLAGS += -DMODULE_INIT_OFFSET=0x190 -DMODULE_EXIT_OFFSET=0x3c8
else ifeq ($(KVER_MAJOR).$(KVER_MINOR),5.15)
FS_CFLAGS += -DMODULE_INIT_OFFSET=0x190 -DMODULE_EXIT_OFFSET=0x3c8
endif

# Source files for freestanding build
FS_TEST_SRCS := test_main.c test_hook_kernel.c kmod_mem_ops.c kmod_log.c kmod_transit_setup.c
FS_ASM_SRCS  := kmod_plt_stub.S
FS_CORE_SRCS := $(KH_ROOT)/src/core_user.c \
                $(KH_ROOT)/src/hmem.c \
                $(KH_ROOT)/src/ksyms.c \
                $(KH_ROOT)/src/arch/arm64/inline.c \
                $(KH_ROOT)/src/arch/arm64/transit.c \
                $(KH_ROOT)/src/arch/arm64/insn.c \
                $(KH_ROOT)/src/arch/arm64/pgtable.c

FS_TEST_OBJS := $(FS_TEST_SRCS:.c=.fs.o)
FS_ASM_OBJS  := $(FS_ASM_SRCS:.S=.fs.o)
FS_CORE_OBJS := $(patsubst $(KH_ROOT)/%.c,%.fs.o,$(FS_CORE_SRCS))

.PHONY: all clean test freestanding

# ---- Approach A: Kbuild ----
all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

# ---- Approach B: Freestanding ----
freestanding: kh_test.ko kmod_loader
	@echo "Freestanding kh_test.ko built successfully"
	@file kh_test.ko
	@$(CROSS_COMPILE)objdump -s -j .modinfo $< 2>/dev/null | tail -5 || true

# ---- Probe module (embedded in kmod_loader for binary offset probing) ----
probe_module.ko: probe_module.c kmod_plt_stub.S
	$(CC) $(FS_CFLAGS) -DMODULE_NAME='"kh_probe"' -c probe_module.c -o probe_module.fs.o
	$(CC) $(FS_CFLAGS) -c kmod_plt_stub.S -o probe_plt.fs.o
	$(LD) -r -T $(KH_ROOT)/kmod/lds/kmod.lds -o $@ probe_module.fs.o probe_plt.fs.o

probe_module_embed.h: probe_module.ko
	@echo "// Auto-generated — do not edit" > $@
	@echo "static const unsigned char probe_ko_data[] = {" >> $@
	@xxd -i < $< >> $@
	@echo "};" >> $@
	@echo "static const unsigned int probe_ko_size = sizeof(probe_ko_data);" >> $@

# ---- kmod_loader: adaptive module loader with embedded probe ----
kmod_loader: kmod_loader.c probe_module_embed.h
	$(CC) -DEMBED_PROBE_KO -static -O2 -o $@ $<

%.fs.o: %.c
	$(CC) $(FS_CFLAGS) -c $< -o $@

src/%.fs.o: $(KH_ROOT)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(FS_CFLAGS) -c $< -o $@

%.fs.o: %.S
	$(CC) $(FS_CFLAGS) -c $< -o $@

kh_test.ko: $(FS_TEST_OBJS) $(FS_ASM_OBJS) $(FS_CORE_OBJS)
	$(LD) -r -T $(KH_ROOT)/kmod/lds/kmod.lds -o $@ $^

# ---- Test (delegates to project-level Android test runner) ----
test:
	$(KH_ROOT)/scripts/run_android_tests.sh --kmod

# ---- Clean ----
clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean 2>/dev/null || true
	rm -f *.fs.o *.ko kmod_loader probe_module_embed.h
	rm -rf src/
