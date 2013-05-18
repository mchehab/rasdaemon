ACLOCAL_AMFLAGS=-I m4
SUBDIRS = libtrace util man
EXTRA_DIST = misc/rasdaemon.service misc/ras-mc-ctl.service

sbin_PROGRAMS = rasdaemon
rasdaemon_SOURCES = rasdaemon.c ras-events.c ras-mc-handler.c
if WITH_SQLITE3
   rasdaemon_SOURCES += ras-record.c
endif
if WITH_AER
   rasdaemon_SOURCES += ras-aer-handler.c
endif
if WITH_MCE
   rasdaemon_SOURCES += ras-mce-handler.c mce-intel.c mce-amd-k8.c \
			bitfield.c mce-intel-p4-p6.c mce-intel-nehalem.c \
			mce-intel-dunnington.c mce-intel-tulsa.c
endif
rasdaemon_LDADD = -lpthread $(SQLITE3_LIBS) libtrace/libtrace.a

include_HEADERS = config.h  ras-events.h  ras-logger.h  ras-mc-handler.h \
		  ras-record.h bitfield.h
