#!/bin/bash

autoreconf && ./configure

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
make -j1 dist-bzip2 && make -j1 dist-rpm && git tag v$VER -f && git push --tags fedorahosted && make upload && git push --tags
