# Copyright (C) 2013- Mauro Carvalho Chehab <mchehab@kernel.org>
# SPDX-License-Identifier: GPL-2.0

# NOTE: For the file variables, please keep each group of dependencies sorted
# alphabetically. Hopefully, this would make easier to solve merge conflicts

AM_DISTCHECK_CONFIGURE_FLAGS = --enable-all
ACLOCAL_AMFLAGS=-I m4
SUBDIRS = util man

SYSTEMD_SERVICES_IN = misc/rasdaemon.service.in
SYSTEMD_SERVICES_IN += misc/ras-mc-ctl.service.in

SYSTEMD_SERVICES = $(SYSTEMD_SERVICES_IN:.service.in=.service)

SYSLOG_SERVICES_IN = misc/rasdaemon.syslog-ng.in
SYSLOG_SERVICES = $(SYSLOG_SERVICES_IN:.syslog-ng.in=.syslog-ng)

LOGROTATE_SERVICES_IN = misc/rasdaemon.logrotate.in
LOGROTATE_SERVICES = $(LOGROTATE_SERVICES_IN:.logrotate.in=.logrotate)

RSYSLOG_SERVICES_IN = misc/rasdaemon.rsyslog.in
RSYSLOG_SERVICES = $(RSYSLOG_SERVICES_IN:.rsyslog.in=.rsyslog)

EXTRA_DIST = $(LOGROTATE_SERVICES_IN)
EXTRA_DIST += $(RSYSLOG_SERVICES_IN)
EXTRA_DIST += $(SYSLOG_SERVICES_IN)
EXTRA_DIST += $(SYSTEMD_SERVICES_IN)

EXTRA_DIST += completions/ras-mc-ctl.bash
EXTRA_DIST += completions/ras-mc-ctl.zsh
EXTRA_DIST += contrib/mem_fail_trigger
EXTRA_DIST += contrib/mc_event_trigger
EXTRA_DIST += labels
EXTRA_DIST += misc/rasdaemon.env

CLEANFILES = misc/rasdaemon.logrotate
CLEANFILES += misc/rasdaemon.rsyslog
CLEANFILES += misc/rasdaemon.service
CLEANFILES += misc/rasdaemon.syslog-ng

CLEANFILES += misc/ras-mc-ctl.service

DISTCLEANFILES = misc/rasdaemon.spec

# This rule is needed because \@sbindir\@ is expanded to \${exec_prefix\}/sbin
# during ./configure phase, therefore it is not possible to add .service.in
# files to AC_CONFIG_FILES in configure.ac

SUFFIXES  = .logrotate .logrotate.in
SUFFIXES += .rsyslog   .rsyslog.in
SUFFIXES += .service   .service.in
SUFFIXES += .syslog-ng .syslog-ng.in

.service.in.service:
	sed -e s,\@sbindir\@,$(sbindir),g -e s,\@SYSCONFDEFDIR\@,@SYSCONFDEFDIR@,g $< > $@

.logrotate.in.logrotate:
	sed -e s,\@sbindir\@,$(sbindir),g -e s,\@localstatedir\@,${localstatedir},g $< > $@

.syslog-ng.in.syslog-ng:
	sed -e s,\@sbindir\@,$(sbindir),g -e s,\@localstatedir\@,${localstatedir},g $< > $@

.rsyslog.in.rsyslog:
	sed -e s,\@sbindir\@,$(sbindir),g -e s,\@localstatedir\@,${localstatedir},g $< > $@

# This rule is needed because the service files must be generated on target
# system after ./configure phase
all-local: $(SYSTEMD_SERVICES) $(SYSLOG_SERVICES) $(RSYSLOG_SERVICES) $(LOGROTATE_SERVICES)

sbin_PROGRAMS = rasdaemon unittest

# Main program
rasdaemon_SOURCES = core/rasdaemon.c

# Unit tests
unittest_SOURCES = tests/main.c
unittest_SOURCES += tests/test-modules.c

# Rasdaemon libraries is where the code actually sits
noinst_LIBRARIES = librasdaemon.a

librasdaemon_a_SOURCES = core/bitfield.c
librasdaemon_a_SOURCES += core/modules.c
librasdaemon_a_SOURCES += core/ras-logger.c
librasdaemon_a_SOURCES += core/ras-events.c
librasdaemon_a_SOURCES += core/ras-mc-handler.c
librasdaemon_a_SOURCES += core/trigger.c
librasdaemon_a_SOURCES += core/types.c

# Keep the IFs alphabetically sorted

if WITH_ABRT_REPORT
   librasdaemon_a_SOURCES += modules/ras-report.c
endif

if WITH_AER
   librasdaemon_a_SOURCES += modules/ras-aer-handler.c
endif

if WITH_AMP_NS_DECODE
   librasdaemon_a_SOURCES += events-arch-arm/non-standard-ampere.c
endif

if WITH_ARM
   librasdaemon_a_SOURCES += events-arch-arm/ras-arm-handler.c
endif

if WITH_CPU_FAULT_ISOLATION
   librasdaemon_a_SOURCES += modules/ras-cpu-isolation.c
   librasdaemon_a_SOURCES += core/queue.c
endif

if WITH_CXL
   librasdaemon_a_SOURCES += events/ras-cxl-handler.c
endif

if WITH_DEVLINK
   librasdaemon_a_SOURCES += events/ras-devlink-handler.c
endif

if WITH_DISKERROR
   librasdaemon_a_SOURCES += events/ras-diskerror-handler.c
endif

if WITH_ERST
   librasdaemon_a_SOURCES += events-arch-x86/ras-erst.c
endif

if WITH_EXTLOG
   librasdaemon_a_SOURCES += core/ras-extlog-handler.c
endif

if WITH_HISI_NS_DECODE
   librasdaemon_a_SOURCES += events-arch-arm/non-standard-hisi_hip08.c
   librasdaemon_a_SOURCES += events-arch-arm/non-standard-hisilicon.c
endif

if WITH_JAGUAR_NS_DECODE
   librasdaemon_a_SOURCES += events-arch-arm/non-standard-jaguarmicro.c
endif

if WITH_MCE
   librasdaemon_a_SOURCES += events-arch-x86/mce-amd.c
   librasdaemon_a_SOURCES += events-arch-x86/mce-amd-k8.c
   librasdaemon_a_SOURCES += events-arch-x86/mce-amd-smca.c
   librasdaemon_a_SOURCES += events-arch-x86/mce-intel.c
   librasdaemon_a_SOURCES += events-arch-x86/mce-intel-broadwell-de.c
   librasdaemon_a_SOURCES += events-arch-x86/mce-intel-broadwell-epex.c
   librasdaemon_a_SOURCES += events-arch-x86/mce-intel-diamond.c
   librasdaemon_a_SOURCES += events-arch-x86/mce-intel-dunnington.c
   librasdaemon_a_SOURCES += events-arch-x86/mce-intel-haswell.c
   librasdaemon_a_SOURCES += events-arch-x86/mce-intel-granite.c
   librasdaemon_a_SOURCES += events-arch-x86/mce-intel-i10nm.c
   librasdaemon_a_SOURCES += events-arch-x86/mce-intel-ivb.c
   librasdaemon_a_SOURCES += events-arch-x86/mce-intel-knl.c
   librasdaemon_a_SOURCES += events-arch-x86/mce-intel-nehalem.c
   librasdaemon_a_SOURCES += events-arch-x86/mce-intel-p4-p6.c
   librasdaemon_a_SOURCES += events-arch-x86/mce-intel-sb.c
   librasdaemon_a_SOURCES += events-arch-x86/mce-intel-skylake-xeon.c
   librasdaemon_a_SOURCES += events-arch-x86/mce-intel-tulsa.c
   librasdaemon_a_SOURCES += events-arch-x86/mce-zhaoxin.c
   librasdaemon_a_SOURCES += events-arch-x86/mce-zhaoxin-kh50000.c
   librasdaemon_a_SOURCES += events-arch-x86/ras-mce-handler.c
endif

if WITH_MEMORY_FAILURE
   librasdaemon_a_SOURCES += events/ras-memory-failure-handler.c
endif

if WITH_NVIDIA_NS_DECODE
   librasdaemon_a_SOURCES += events-arch-arm/non-standard-nvidia.c
endif

if WITH_NON_STANDARD
   librasdaemon_a_SOURCES += events-arch-arm/ras-non-standard-handler.c
endif

if WITH_OPENBMC_UNIFIED_SEL
   librasdaemon_a_SOURCES += events/unified-sel.c
endif

if WITH_PFA
   librasdaemon_a_SOURCES += core/rbtree.c modules/ras-page-isolation.c
endif

if WITH_POISON_PAGE_STAT
   librasdaemon_a_SOURCES += events/ras-poison-page-stat.c
endif

if WITH_SIGNAL
   librasdaemon_a_SOURCES += events/ras-signal-handler.c
endif

if WITH_SQLITE3
   librasdaemon_a_SOURCES += db/db-sqlite3.c
   librasdaemon_a_SOURCES += db/ras-record.c
endif

if WITH_YITIAN_NS_DECODE
   librasdaemon_a_SOURCES += events-arch-arm/non-standard-yitian.c
endif

if WITH_RERI
   librasdaemon_a_SOURCES += events/ras-reri-handler.c
endif

LDADD = -lpthread $(SQLITE3_LIBS) $(LIBTRACEEVENT_LIBS) $(LIBPCI_LIBS) librasdaemon.a
CFLAGS = $(SQLITE3_CFLAGS) $(LIBTRACEEVENT_CFLAGS) $(LIBPCI_CFLAGS)

include_HEADERS = config.h

include_HEADERS += core/bitfield.h
include_HEADERS += core/queue.h
include_HEADERS += core/rbtree.h
include_HEADERS += core/trigger.h
include_HEADERS += core/types.h
include_HEADERS += events/unified-sel.h

include_HEADERS += events-arch-arm/non-standard-ampere.h
include_HEADERS += events-arch-arm/non-standard-hisilicon.h
include_HEADERS += events-arch-arm/non-standard-jaguarmicro.h
include_HEADERS += events-arch-arm/non-standard-nvidia.h
include_HEADERS += events-arch-arm/non-standard-yitian.h

include_HEADERS += modules/ras-aer-handler.h
include_HEADERS += events-arch-arm/ras-arm-handler.h
include_HEADERS += modules/ras-cpu-isolation.h
include_HEADERS += events/ras-cxl-handler.h
include_HEADERS += events/ras-devlink-handler.h
include_HEADERS += events/ras-diskerror-handler.h
include_HEADERS += events-arch-x86/ras-erst.h
include_HEADERS += core/ras-events.h
include_HEADERS += core/ras-extlog-handler.h
include_HEADERS += core/ras-logger.h
include_HEADERS += core/ras-mc-handler.h
include_HEADERS += events-arch-x86/ras-mce-handler.h
include_HEADERS += events/ras-memory-failure-handler.h
include_HEADERS += events-arch-arm/ras-non-standard-handler.h
include_HEADERS += modules/ras-page-isolation.h
include_HEADERS += events/ras-poison-page-stat.h
include_HEADERS += db/ras-record.h
include_HEADERS += modules/ras-report.h
include_HEADERS += events/ras-signal-handler.h
include_HEADERS += events/ras-reri-handler.h

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
	mock --resultdir="./SRPMS" `rpm --eval %{_topdir}`/SRPMS/@PACKAGE@-@PACKAGE_VERSION@*.src.rpm

rpmlint:
	rpmlint misc/@PACKAGE@.spec `rpm --eval %{_topdir}`/SRPMS/@PACKAGE@-@PACKAGE_VERSION@*.src.rpm `rpm --eval %{_topdir}`/RPMS/*/@PACKAGE@-@PACKAGE_VERSION@*.rpm

upload:
	scp `rpm --eval %{_topdir}`/SRPMS/@PACKAGE@-@PACKAGE_VERSION@*.src.rpm @PACKAGE@-@PACKAGE_VERSION@.tar.bz2 misc/rasdaemon.spec www.infradead.org:public_html/rasdaemon

test: unittest librasdaemon.a
	./unittest

# custom target
install-data-local:
	$(install_sh) -d "$(DESTDIR)@sysconfdir@/ras/dimm_labels.d"
	for f in @abs_srcdir@/labels/*; do \
		test -f "$$f" || continue; \
		$(install_sh) -m 0644 "$$f" "$(DESTDIR)@sysconfdir@/ras/dimm_labels.d/"; \
	done
	$(install_sh) -d "$(DESTDIR)@sysconfdir@/ras/triggers"
	install -D -p -m 0655 @abs_srcdir@/misc/rasdaemon.env "$(DESTDIR)@SYSCONFDEFDIR@/rasdaemon"
	$(install_sh) @abs_srcdir@/contrib/mc_event_trigger "$(DESTDIR)@sysconfdir@/ras/triggers/mc_event_trigger"
	$(install_sh) @abs_srcdir@/contrib/mem_fail_trigger "$(DESTDIR)@sysconfdir@/ras/triggers/mem_fail_trigger"
	if [ -d "$(DESTDIR)@sysconfdir@/syslog-ng/conf.d" ]; then \
		install -D -p -m 0655 @abs_srcdir@/misc/rasdaemon.syslog-ng "$(DESTDIR)@sysconfdir@/syslog-ng/conf.d/rasdaemon.conf"; \
	fi
	if [ -d "$(DESTDIR)@sysconfdir@/rsyslog.d/" ]; then \
		install -D -p -m 0655 @abs_srcdir@/misc/rasdaemon.rsyslog "$(DESTDIR)@sysconfdir@/rsyslog.d/rasdaemon.conf"; \
	fi
	if [ -d "$(DESTDIR)@sysconfdir@/logrotate.d" ]; then \
		install -D -p -m 0655 @abs_srcdir@/misc/rasdaemon.logrotate "$(DESTDIR)@sysconfdir@/logrotate.d/rasdaemon"; \
	fi
	$(install_sh) -d "$(DESTDIR)${datarootdir}/zsh/site-functions"
	install -D -p -m 0644 @abs_srcdir@/completions/ras-mc-ctl.zsh \
		$(DESTDIR)${datarootdir}/zsh/site-functions/_ras-mc-ctl
	$(install_sh) -d "$(DESTDIR)${datarootdir}/bash-completion/completions"
	install -D -p -m 0644 @abs_srcdir@/completions/ras-mc-ctl.bash \
		$(DESTDIR)${datarootdir}/bash-completion/completions/ras-mc-ctl
