/* SPDX-License-Identifier: GPL-2.0-or-later */

/*
 * Copyright (C) 2013 Petr Holasek <pholasek@redhat.com>
 */

#ifndef __RAS_LOGGER_H

#include <syslog.h>

#include "types.h"

/*
 * Logging macros
 */

#ifndef TOOL_NAME
	#define TOOL_NAME "rasdaemon"
#endif

#define SYSLOG	BIT(0)
#define TERM	BIT(1)
#define ALL	(SYSLOG | TERM)
/* TODO: global logging limit mask */

#define log(where, level, fmt, args...) do {			\
	if ((where) & SYSLOG)					\
		syslog(level, fmt, ##args);			\
	if ((where) & TERM) {					\
		fprintf(stderr, "%s: ", TOOL_NAME);		\
		fprintf(stderr, fmt, ##args);			\
		fflush(stderr);					\
	}							\
} while (0)

#define __RAS_LOGGER_H
#endif
