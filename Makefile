# SPDX-License-Identifier: GPL-2.0-only
#
# Copyright (C) 2026 Mauro Carvalho Chehab <mchehab+huawei@kernel.org>

default: all

PYTHON ?= python3

build/Makefile.inc: Makefile.inc.in all

-include build/Makefile.inc

build/build.ninja:
	@if test -d build/; then \
		meson setup build --reconfigure; \
	else \
		meson setup build; \
	fi

all: build/build.ninja
	@ninja -C build

clean: build/build.ninja
	@ninja -C build clean

reconfigure: build/build.ninja
	@ninja -C build reconfigure

install: build/build.ninja
	@ninja -C build install

uninstall: build/build.ninja
	@ninja -C build uninstall

python-test:
	PYTHONDONTWRITEBYTECODE=1 $(PYTHON) tests/run.py
