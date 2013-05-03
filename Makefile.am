ACLOCAL_AMFLAGS=-I m4
SUBDIRS = libtrace
EXTRA_DIST = misc/rasdaemon.service

sbin_PROGRAMS = rasdaemon
rasdaemon_SOURCES = rasdaemon.c ras-record.c ras-events.c ras-mc-handler.c
rasdaemon_LDADD = -lpthread -lsqlite3 libtrace/libtrace.a
