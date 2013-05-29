ACLOCAL_AMFLAGS=-I m4
SUBDIRS = libtrace util man
EXTRA_DIST = misc/rasdaemon.service misc/ras-mc-ctl.service

sbin_PROGRAMS = rasdaemon
rasdaemon_SOURCES = rasdaemon.c ras-events.c ras-mc-handler.c \
		    bitfield.c
if WITH_SQLITE3
   rasdaemon_SOURCES += ras-record.c
endif
if WITH_AER
   rasdaemon_SOURCES += ras-aer-handler.c
endif
if WITH_MCE
   rasdaemon_SOURCES += ras-mce-handler.c mce-intel.c mce-amd-k8.c \
			mce-intel-p4-p6.c mce-intel-nehalem.c \
			mce-intel-dunnington.c mce-intel-tulsa.c \
			mce-intel-sb.c mce-intel-ivb.c
endif
rasdaemon_LDADD = -lpthread $(SQLITE3_LIBS) libtrace/libtrace.a

include_HEADERS = config.h  ras-events.h  ras-logger.h  ras-mc-handler.h \
		  ras-aer-handler.h ras-mce-handler.h ras-record.h bitfield.h

# This rule can't be called with more than one Makefile job (like make -j8)
# I can't figure out a way to fix that
dist-rpm: dist-bzip2
	cp @PACKAGE@-@PACKAGE_VERSION@.tar.bz2 `rpm --eval %{_topdir}`/SOURCES/
	rpmbuild -ba misc/@PACKAGE@.spec
	cp `rpm --eval %{_topdir}`/SRPMS/@PACKAGE@-@PACKAGE_VERSION@*.src.rpm .

rpmlint:
	rpmlint misc/@PACKAGE@.spec `rpm --eval %{_topdir}`/SRPMS/@PACKAGE@-@PACKAGE_VERSION@*.src.rpm `rpm --eval %{_topdir}`/RPMS/*/@PACKAGE@-@PACKAGE_VERSION@*.rpm

upload:
	scp @PACKAGE@-@PACKAGE_VERSION@*.src.rpm @PACKAGE@-@PACKAGE_VERSION@.tar.bz2 misc/rasdaemon.spec fedorapeople.org:public_html

# custom target
install-data-local:
	$(install_sh) -d "@RASSTATEDIR@"
