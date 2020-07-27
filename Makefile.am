ACLOCAL_AMFLAGS=-I m4
SUBDIRS = libtrace util man
SYSTEMD_SERVICES_IN = misc/rasdaemon.service.in misc/ras-mc-ctl.service.in
SYSTEMD_SERVICES = $(SYSTEMD_SERVICES_IN:.service.in=.service)
EXTRA_DIST = $(SYSTEMD_SERVICES_IN) misc/rasdaemon.env

# This rule is needed because \@sbindir\@ is expanded to \${exec_prefix\}/sbin
# during ./configure phase, therefore it is not possible to add .service.in
# files to AC_CONFIG_FILES in configure.ac
SUFFIXES = .service.in .service
.service.in.service:
	sed -e s,\@sbindir\@,$(sbindir),g $< > $@

# This rule is needed because the service files must be generated on target
# system after ./configure phase
all-local: $(SYSTEMD_SERVICES)

sbin_PROGRAMS = rasdaemon
rasdaemon_SOURCES = rasdaemon.c ras-events.c ras-mc-handler.c \
		    bitfield.c
if WITH_SQLITE3
   rasdaemon_SOURCES += ras-record.c
endif
if WITH_AER
   rasdaemon_SOURCES += ras-aer-handler.c
endif
if WITH_NON_STANDARD
   rasdaemon_SOURCES += ras-non-standard-handler.c
endif
if WITH_ARM
   rasdaemon_SOURCES += ras-arm-handler.c
endif
if WITH_MCE
   rasdaemon_SOURCES += ras-mce-handler.c mce-intel.c mce-amd.c \
			mce-intel-p4-p6.c mce-intel-nehalem.c \
			mce-intel-dunnington.c mce-intel-tulsa.c \
			mce-intel-sb.c mce-intel-ivb.c mce-intel-haswell.c \
			mce-intel-knl.c mce-intel-broadwell-de.c \
			mce-intel-broadwell-epex.c mce-intel-skylake-xeon.c \
			mce-amd-k8.c mce-amd-smca.c
endif
if WITH_EXTLOG
   rasdaemon_SOURCES += ras-extlog-handler.c
endif
if WITH_DEVLINK
   rasdaemon_SOURCES += ras-devlink-handler.c
endif
if WITH_DISKERROR
   rasdaemon_SOURCES += ras-diskerror-handler.c
endif
if WITH_ABRT_REPORT
   rasdaemon_SOURCES += ras-report.c
endif
if WITH_HISI_NS_DECODE
   rasdaemon_SOURCES += non-standard-hisi_hip08.c
endif
if WITH_MEMORY_CE_PFA
   rasdaemon_SOURCES += rbtree.c ras-page-isolation.c
endif
rasdaemon_LDADD = -lpthread $(SQLITE3_LIBS) libtrace/libtrace.a

include_HEADERS = config.h  ras-events.h  ras-logger.h  ras-mc-handler.h \
		  ras-aer-handler.h ras-mce-handler.h ras-record.h bitfield.h ras-report.h \
		  ras-extlog-handler.h ras-arm-handler.h ras-non-standard-handler.h \
		  ras-devlink-handler.h ras-diskerror-handler.h rbtree.h ras-page-isolation.h

# This rule can't be called with more than one Makefile job (like make -j8)
# I can't figure out a way to fix that
dist-rpm: dist-bzip2
	if [ ! -d "`rpm --eval %{_topdir}`/SOURCES/" ]; then mkdir "`rpm --eval %{_topdir}`/SOURCES/"; fi
	cp @PACKAGE@-@PACKAGE_VERSION@.tar.bz2 `rpm --eval %{_topdir}`/SOURCES/
	rpmbuild -ba misc/@PACKAGE@.spec
	cp `rpm --eval %{_topdir}`/SRPMS/@PACKAGE@-@PACKAGE_VERSION@*.src.rpm .

srpm: dist-bzip2
	if [ ! -d "`rpm --eval %{_topdir}`/SOURCES/" ]; then mkdir "`rpm --eval %{_topdir}`/SOURCES/"; fi
	cp @PACKAGE@-@PACKAGE_VERSION@.tar.bz2 `rpm --eval %{_topdir}`/SOURCES/
	rpmbuild -bs misc/@PACKAGE@.spec

mock: srpm
	mock `rpm --eval %{_topdir}`/SRPMS/@PACKAGE@-@PACKAGE_VERSION@*.src.rpm

rpmlint:
	rpmlint misc/@PACKAGE@.spec `rpm --eval %{_topdir}`/SRPMS/@PACKAGE@-@PACKAGE_VERSION@*.src.rpm `rpm --eval %{_topdir}`/RPMS/*/@PACKAGE@-@PACKAGE_VERSION@*.rpm

upload:
	scp `rpm --eval %{_topdir}`/SRPMS/@PACKAGE@-@PACKAGE_VERSION@*.src.rpm @PACKAGE@-@PACKAGE_VERSION@.tar.bz2 misc/rasdaemon.spec www.infradead.org:public_html/rasdaemon

# custom target
install-data-local:
	$(install_sh) -d "$(DESTDIR)@RASSTATEDIR@"
	$(install_sh) -d "$(DESTDIR)@sysconfdir@/ras/dimm_labels.d"
if WITH_MEMORY_CE_PFA
	$(install_sh) @abs_srcdir@/misc/rasdaemon.env "$(DESTDIR)@sysconfdir@/sysconfig/rasdaemon"
endif
