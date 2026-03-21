# SPDX-License-Identifier: GPL-2.0-or-later
# KernelHook Makefile

ARCH ?= arm64

INCLUDE_DIR := include

ifeq ($(ARCH),arm64)
    CROSS_COMPILE ?= aarch64-linux-gnu-
    ARCH_DIR := arch/arm64
    LDSCRIPT := kpimg.lds
else ifeq ($(ARCH),x86_64)
    CROSS_COMPILE ?= x86_64-linux-gnu-
    ARCH_DIR := arch/x86_64
    LDSCRIPT := kpimg_x86.lds
else
    $(error Unsupported ARCH=$(ARCH). Use arm64 or x86_64)
endif

CC := $(CROSS_COMPILE)gcc
LD := $(CROSS_COMPILE)ld
OBJCOPY := $(CROSS_COMPILE)objcopy

CFLAGS := -nostdlib -ffreestanding -fno-builtin \
          -fno-stack-protector -fno-common \
          -I$(INCLUDE_DIR) -I$(INCLUDE_DIR)/$(ARCH_DIR) \
          -Wall -Wextra -Werror \
          -O2

ifeq ($(ARCH),arm64)
    CFLAGS += -march=armv8-a
endif

# Source files
CORE_SRCS := $(wildcard src/*.c)
ARCH_SRCS := $(wildcard src/$(ARCH_DIR)/*.c)
SRCS := $(CORE_SRCS) $(ARCH_SRCS)
OBJS := $(SRCS:.c=.o)

TARGET := kpimg.bin

.PHONY: all clean typecheck

all: $(TARGET)

$(TARGET): $(OBJS) $(LDSCRIPT)
	$(LD) -T $(LDSCRIPT) -o kpimg.elf $(OBJS)
	$(OBJCOPY) -O binary kpimg.elf $(TARGET)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

typecheck:
	@echo "Running typecheck (syntax-only) for ARCH=$(ARCH)..."
ifeq ($(ARCH),arm64)
	@for f in $(SRCS); do \
		echo "  Checking $$f"; \
		clang --target=aarch64-linux-gnu $(CFLAGS) -fsyntax-only $$f || exit 1; \
	done
else ifeq ($(ARCH),x86_64)
	@for f in $(SRCS); do \
		echo "  Checking $$f"; \
		clang --target=x86_64-linux-gnu $(CFLAGS) -fsyntax-only $$f || exit 1; \
	done
endif
	@echo "Typecheck passed."

clean:
	rm -f $(OBJS) kpimg.elf $(TARGET)
