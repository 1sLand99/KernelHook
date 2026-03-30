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

# Freestanding build flags
FS_CFLAGS := -DKMOD_FREESTANDING \
             -ffreestanding -fno-builtin -fno-stack-protector -fno-common \
             -fno-PIE -fno-pic \
             -I$(KH_ROOT)/include -I$(KH_ROOT)/include/arch/arm64 \
             -I$(PWD) \
             -march=armv8-a -O2 -Wall -Wextra -Werror \
             -Wno-unused-parameter

# Source files for freestanding build
FS_TEST_SRCS := test_main.c test_hook_kernel.c kmod_mem_ops.c kmod_log.c
FS_CORE_SRCS := $(KH_ROOT)/src/hmem.c \
                $(KH_ROOT)/src/ksyms.c \
                $(KH_ROOT)/src/arch/arm64/inline.c \
                $(KH_ROOT)/src/arch/arm64/transit.c \
                $(KH_ROOT)/src/arch/arm64/insn.c \
                $(KH_ROOT)/src/arch/arm64/pgtable.c

FS_TEST_OBJS := $(FS_TEST_SRCS:.c=.fs.o)
FS_CORE_OBJS := $(patsubst $(KH_ROOT)/%.c,%.fs.o,$(FS_CORE_SRCS))

.PHONY: all clean test freestanding

# ---- Approach A: Kbuild ----
all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

# ---- Approach B: Freestanding ----
freestanding: kh_test.ko
	@echo "Freestanding kh_test.ko built successfully"
	@file kh_test.ko
	@$(CROSS_COMPILE)objdump -s -j .modinfo $< 2>/dev/null | tail -5 || true

%.fs.o: %.c
	$(CC) $(FS_CFLAGS) -c $< -o $@

src/%.fs.o: $(KH_ROOT)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(FS_CFLAGS) -c $< -o $@

kh_test.ko: $(FS_TEST_OBJS) $(FS_CORE_OBJS)
	$(LD) -r -T kmod_freestanding.lds -o $@ $^

# ---- Test (delegates to project-level Android test runner) ----
test:
	$(KH_ROOT)/scripts/run_android_tests.sh --kmod

# ---- Clean ----
clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean 2>/dev/null || true
	rm -f *.fs.o *.ko
	rm -rf src/
