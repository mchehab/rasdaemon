#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-only
#
# Copyright (C) 2013-s2026 Mauro Carvalho Chehab <mchehab+huawei@kernel.org>

DRY_RUN=0
NO_TAGS=0
while [ "$#" -gt 0 ]; do
	case "$1" in
		--dry-run|-n)
			DRY_RUN=1
			;;
		--no-tags)
			NO_TAGS=1
			;;
		--help|-h)
			echo "Usage: $0 [--dry-run|-n] [--no-tags]"
			exit 0
			;;
		*)
			echo "Usage: $0 [--dry-run|-n] [--no-tags]" >&2
			exit 1
			;;
	esac
	shift
done

catch() {
	echo "Error on line $1: $2"
        exit 1
}
trap 'catch $LINENO "$BASH_COMMAND"' ERR

VER=$(perl -ne 'print "$1\n" if /\bversion:\s*\x27(\d[\d\.]+)\x27/' meson.build)
if [ "x$VER" == "x" ]; then
	echo "Can't parse rasdaemon version"
	exit 1
fi
TAG="v$VER"

if [ "$DRY_RUN" -ne 1 ] && [ "$NO_TAGS" -ne 1 ]; then
	echo
	echo "************************************************************************"
	echo "Creating a new release tag"
	echo "************************************************************************"

	if git show-ref --verify --quiet "refs/tags/$TAG"; then
		TAG_COMMIT=$(git rev-parse --verify "${TAG}^{commit}")
		HEAD_COMMIT=$(git rev-parse --verify HEAD)
		if [ "$TAG_COMMIT" != "$HEAD_COMMIT" ]; then
			echo "Error: tag ${TAG} already exists and points to a different commit. If you want to test, you can use:"
			echo "   \$ $0 --dry-run"
			echo "or:"
			echo "   \$ $0 --no-tags"
			exit 1
		fi
		echo "Tag ${TAG} already exists and points to HEAD; leaving it unchanged."
	else
		git tag "$TAG"
	fi
fi

if [ "$NO_TAGS" -eq 1 ]; then
	echo "Skipping release tag creation (--no-tags)."
fi

make distclean
make build/build.ninja
make

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
