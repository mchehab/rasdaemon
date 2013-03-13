ACLOCAL_AMFLAGS=-I m4
SUBDIRS = libtrace

bin_PROGRAMS = rasdaemon
rasdaemon_SOURCES = rasdaemon.c ras-record.c ras-mc-event.c
rasdaemon_LDADD = -lpthread -lsqlite3 libtrace/libtrace.a
