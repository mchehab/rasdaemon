# SPDX-License-Identifier: GPL-2.0
#
# Copyright (C) 2026 Mauro Carvalho Chehab <mchehab+huawei@kernel.org>

default: all

PYTHON ?= python3

-include build/Makefile.inc

.PHONY: python-test

python-test:
	PYTHONDONTWRITEBYTECODE=1 $(PYTHON) -m unittest discover \
		-s tests -p 'test_*.py' -v

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
