ACLOCAL_AMFLAGS=-I m4
SUBDIRS = libtrace
EXTRA_DIST = misc/rasdaemon.service

sbin_PROGRAMS = rasdaemon
rasdaemon_SOURCES = rasdaemon.c ras-events.c ras-mc-handler.c
if WITH_SQLITE3
   rasdaemon_SOURCES += ras-record.c
endif
rasdaemon_LDADD = -lpthread $(SQLITE3_LIBS) libtrace/libtrace.a
