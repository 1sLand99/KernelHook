# SPDX-License-Identifier: GPL-2.0-or-later
# Host build for kh_crc. Builds with the system cc; no cross-compiler.

CC      ?= cc
CFLAGS  ?= -O2 -Wall -Wextra -Werror -std=c99 -pedantic
PREFIX  ?= /usr/local

.PHONY: all test clean install

all: kh_crc

kh_crc: kh_crc.c
	$(CC) $(CFLAGS) -o $@ $<

test: kh_crc
	./tests/run_tests.sh

clean:
	rm -f kh_crc *.o
