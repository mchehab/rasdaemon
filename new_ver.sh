#!/bin/bash

autoreconf && ./configure --enable-sqlite3 --enable-aer --enable-mce --enable-extlog --enable-abrt-report

VER="`perl -ne 'print "$1\n" if (/Version:\s*(.*)/);' misc/rasdaemon.spec`"
if [ "x$VER" == "x" ]; then
	echo "Can't parse rasdaemon version"
	exit -1
fi
echo
echo "************************************************************************"
echo "Building RPM files for version: $VER"
echo "************************************************************************"
echo
git tag v$VER -f && make mock && make upload && git push --tags master
