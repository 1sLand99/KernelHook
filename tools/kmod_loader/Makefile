# SPDX-License-Identifier: GPL-2.0-or-later
# Build kmod_loader — adaptive kernel module loader with embedded probe.
#
# Usage:
#   make                    — build kmod_loader (uses pre-committed probe_module_embed.h)
#   make regenerate         — regenerate probe_module_embed.h from source (needs cross-compiler)
#   make clean              — remove build artifacts

KH_ROOT := $(shell cd ../.. && pwd)

# Cross-compiler (only needed for 'regenerate' target)
CROSS_COMPILE ?= aarch64-linux-gnu-

# Auto-detect NDK if default gcc is missing
_KL_HAVE_GCC := $(shell which $(CROSS_COMPILE)gcc 2>/dev/null)
ifeq ($(_KL_HAVE_GCC),)
  _KL_NDK_CANDIDATES := $(wildcard $(HOME)/Library/Android/sdk/ndk/*) \
                         $(wildcard $(ANDROID_NDK_ROOT)) \
                         $(wildcard $(ANDROID_HOME)/ndk/*)
  _KL_NDK_BASE := $(lastword $(filter-out %.zip,$(_KL_NDK_CANDIDATES)))
  ifneq ($(_KL_NDK_BASE),)
    _KL_NDK_PREBUILT := $(firstword $(wildcard $(_KL_NDK_BASE)/toolchains/llvm/prebuilt/*/bin))
    ifneq ($(_KL_NDK_PREBUILT),)
      CROSS_CC  := $(_KL_NDK_PREBUILT)/clang --target=aarch64-linux-android30
      CROSS_LD  := $(_KL_NDK_PREBUILT)/ld.lld
    endif
  endif
endif

CROSS_CC ?= $(CROSS_COMPILE)gcc
CROSS_LD ?= $(CROSS_COMPILE)ld

# Host compiler for kmod_loader itself
HOSTCC ?= cc

FS_CFLAGS := -DKMOD_FREESTANDING \
             -ffreestanding -fno-builtin -fno-stack-protector -fno-common \
             -fno-PIE -fno-pic \
             -I$(KH_ROOT)/include -I$(KH_ROOT)/include/arch/arm64 \
             -march=armv8.5-a -O2 -Wall -Wextra -Werror \
             -Wno-unused-parameter -Wno-unused-function

.PHONY: all regenerate clean

# Default: build kmod_loader using pre-committed probe_module_embed.h
all: kmod_loader
	@echo "Built kmod_loader successfully"

kmod_loader: kmod_loader.c probe_module_embed.h
	$(HOSTCC) -DEMBED_PROBE_KO -static -O2 -o $@ $<

# Regenerate probe_module_embed.h from source (requires cross-compiler)
regenerate: probe_module.ko
	@echo "// Auto-generated — do not edit" > probe_module_embed.h
	@echo "static const unsigned char probe_ko_data[] = {" >> probe_module_embed.h
	@xxd -i < probe_module.ko >> probe_module_embed.h
	@echo "};" >> probe_module_embed.h
	@echo "static const unsigned int probe_ko_size = sizeof(probe_ko_data);" >> probe_module_embed.h
	@echo "probe_module_embed.h regenerated — commit if changed"

probe_module.ko: probe_module.c
	$(CROSS_CC) $(FS_CFLAGS) -DMODULE_NAME='"kh_probe"' -c probe_module.c -o probe_module.fs.o
	$(CROSS_CC) $(FS_CFLAGS) -c $(KH_ROOT)/kmod/plt/plt_stub.S -o probe_plt.fs.o
	$(CROSS_LD) -r -T $(KH_ROOT)/kmod/lds/kmod.lds -o $@ probe_module.fs.o probe_plt.fs.o

clean:
	rm -f kmod_loader probe_module.ko probe_module.fs.o probe_plt.fs.o
