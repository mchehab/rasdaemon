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

EXTRA_DIST += contrib/mem_fail_trigger
EXTRA_DIST += contrib/mc_event_trigger
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

sbin_PROGRAMS = rasdaemon

rasdaemon_SOURCES = rasdaemon.c

rasdaemon_SOURCES += bitfield.c
rasdaemon_SOURCES += ras-events.c
rasdaemon_SOURCES += ras-mc-handler.c
rasdaemon_SOURCES += trigger.c
rasdaemon_SOURCES += types.c

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
   rasdaemon_SOURCES += mce-amd.c
   rasdaemon_SOURCES += mce-amd-k8.c
   rasdaemon_SOURCES += mce-amd-smca.c
   rasdaemon_SOURCES += mce-intel.c
   rasdaemon_SOURCES += mce-intel-broadwell-de.c
   rasdaemon_SOURCES += mce-intel-broadwell-epex.c
   rasdaemon_SOURCES += mce-intel-dunnington.c
   rasdaemon_SOURCES += mce-intel-haswell.c
   rasdaemon_SOURCES += mce-intel-i10nm.c
   rasdaemon_SOURCES += mce-intel-ivb.c
   rasdaemon_SOURCES += mce-intel-knl.c
   rasdaemon_SOURCES += mce-intel-nehalem.c
   rasdaemon_SOURCES += mce-intel-p4-p6.c
   rasdaemon_SOURCES += mce-intel-sb.c
   rasdaemon_SOURCES += mce-intel-skylake-xeon.c
   rasdaemon_SOURCES += mce-intel-tulsa.c
   rasdaemon_SOURCES += ras-events.c
   rasdaemon_SOURCES += ras-mc-handler.c
   rasdaemon_SOURCES += ras-mce-handler.c
   rasdaemon_SOURCES += rasdaemon.c
   rasdaemon_SOURCES += trigger.c
   rasdaemon_SOURCES += types.c

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
if WITH_MEMORY_FAILURE
   rasdaemon_SOURCES += ras-memory-failure-handler.c
endif
if WITH_ABRT_REPORT
   rasdaemon_SOURCES += ras-report.c
endif
if WITH_HISI_NS_DECODE
   rasdaemon_SOURCES += non-standard-hisi_hip08.c
   rasdaemon_SOURCES += non-standard-hisilicon.c
endif
if WITH_PFA
   rasdaemon_SOURCES += rbtree.c ras-page-isolation.c
endif
if WITH_AMP_NS_DECODE
   rasdaemon_SOURCES += non-standard-ampere.c
endif
if WITH_CPU_FAULT_ISOLATION
   rasdaemon_SOURCES += ras-cpu-isolation.c
   rasdaemon_SOURCES += queue.c
endif
if WITH_OPENBMC_UNIFIED_SEL
   rasdaemon_SOURCES += unified-sel.c
endif
if WITH_CXL
   rasdaemon_SOURCES += ras-cxl-handler.c
endif
if WITH_YITIAN_NS_DECODE
   rasdaemon_SOURCES += non-standard-yitian.c
endif
if WITH_JAGUAR_NS_DECODE
   rasdaemon_SOURCES += non-standard-jaguarmicro.c
endif
if WITH_SIGNAL
   rasdaemon_SOURCES += ras-signal-handler.c
endif

if WITH_POISON_PAGE_STAT
   rasdaemon_SOURCES += ras-poison-page-stat.c
endif

if WITH_ERST
   rasdaemon_SOURCES += ras-erst.c
endif

rasdaemon_LDADD = -lpthread $(SQLITE3_LIBS) $(LIBTRACEEVENT_LIBS) $(LIBPCI_LIBS)
rasdaemon_CFLAGS = $(SQLITE3_CFLAGS) $(LIBTRACEEVENT_CFLAGS) $(LIBPCI_CFLAGS)


include_HEADERS = config.h

include_HEADERS += bitfield.h
include_HEADERS += queue.h
include_HEADERS += rbtree.h
include_HEADERS += trigger.h
include_HEADERS += types.h
include_HEADERS += unified-sel.h

include_HEADERS += non-standard-ampere.h
include_HEADERS += non-standard-hisilicon.h
include_HEADERS += non-standard-jaguarmicro.h
include_HEADERS += non-standard-yitian.h

include_HEADERS += ras-aer-handler.h
include_HEADERS += ras-arm-handler.h
include_HEADERS += ras-cpu-isolation.h
include_HEADERS += ras-cxl-handler.h
include_HEADERS += ras-devlink-handler.h
include_HEADERS += ras-diskerror-handler.h
include_HEADERS += ras-erst.h
include_HEADERS += ras-events.h
include_HEADERS += ras-extlog-handler.h
include_HEADERS += ras-logger.h
include_HEADERS += ras-mc-handler.h
include_HEADERS += ras-mce-handler.h
include_HEADERS += ras-memory-failure-handler.h
include_HEADERS += ras-non-standard-handler.h
include_HEADERS += ras-page-isolation.h
include_HEADERS += ras-poison-page-stat.h
include_HEADERS += ras-record.h
include_HEADERS += ras-report.h
include_HEADERS += ras-signal-handler.h

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

# custom target
install-data-local:
	$(install_sh) -d "$(DESTDIR)@sysconfdir@/ras/dimm_labels.d"
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
