#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-only
#
# Copyright (C) 2013-s2026 Mauro Carvalho Chehab <mchehab+huawei@kernel.org>

DRY_RUN=0
for arg in "$@"; do
    case "$arg" in
        --dry-run|-n) DRY_RUN=1 ;;
    esac
done

catch() {
	echo "Error on line $1: $2"
        exit 1
}
trap 'catch $LINENO "$BASH_COMMAND"' ERR

make distclean
make build/build.ninja
make

VER=$(perl -ne 'print "$1\n" if /\bversion:\s*\x27(\d[\d\.]+)\x27/' meson.build)
if [ "x$VER" == "x" ]; then
	echo "Can't parse rasdaemon version"
	exit 1
fi
TAG="v$VER"

if [ "$DRY_RUN" -ne 1 ]; then
	echo
	echo "************************************************************************"
	echo "Creating a new release tag"
	echo "************************************************************************"

	if git show-ref --verify --quiet "refs/tags/$TAG"; then
	echo "Error: tag ${TAG} already exists. If you tant to test, you can use:"
	echo "   \$ $0 --dry-run"
	exit 1
	fi

	git tag $TAG -f
fi


echo
echo "************************************************************************"
echo "Running unit tests"
echo "************************************************************************"
make test
make python-test

echo
echo "************************************************************************"
echo "Building RPM files for version: $VER"
echo "************************************************************************"
echo
DRY_RUN=$DRY_RUN  make mock

if [ "$DRY_RUN" -ne 1 ]; then
	make upload
	git push
fi
