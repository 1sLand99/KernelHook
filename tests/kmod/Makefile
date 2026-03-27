# SPDX-License-Identifier: GPL-2.0-or-later
# Convenience Makefile for KernelHook kernel module test harness
#
# Targets:
#   make              — build the test module against the running kernel
#   make KDIR=/path   — build against a specific kernel source tree
#   make clean         — remove build artifacts
#   make test          — load module, capture dmesg output, unload, parse results

KDIR ?= /lib/modules/$(shell uname -r)/build
PWD  := $(shell pwd)

.PHONY: all clean test

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

test: all
	@echo "=== KernelHook Kernel Module Test ==="
	@echo "Loading kh_test.ko..."
	sudo insmod kh_test.ko
	@sleep 1
	@echo "--- dmesg output ---"
	@dmesg | grep -A1 "kh_test" | tail -40
	@echo "--- unloading ---"
	sudo rmmod kh_test
	@echo "--- Parsing results ---"
	@dmesg | grep "kh_test" | grep -c "PASS" | xargs -I{} echo "PASSED: {} tests"
	@FAIL=$$(dmesg | grep "kh_test" | grep -c "FAIL"); \
	 if [ "$$FAIL" -gt 0 ]; then \
	   echo "FAILED: $$FAIL tests"; exit 1; \
	 else \
	   echo "All tests passed."; \
	 fi
