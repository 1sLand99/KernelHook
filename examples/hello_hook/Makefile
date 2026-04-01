# SPDX-License-Identifier: GPL-2.0-or-later
# Build hello_hook.ko example

MODULE_NAME  := hello_hook
MODULE_SRCS  := hello_hook.c
KERNELHOOK_DIR := $(shell cd ../../kmod && pwd)
include $(KERNELHOOK_DIR)/mk/kmod.mk
