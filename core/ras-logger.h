/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2013 Petr Holasek <pholasek@redhat.com>
 */

#ifndef __RAS_LOGGER_H
#define __RAS_LOGGER_H

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

#include "core/types.h"

/*
 * Logging macros
 */

#define binary_name (program_invocation_short_name)

#define SYSLOG	BIT(0)
#define TERM	BIT(1)
#define ALL	(SYSLOG | TERM)

extern bool mock_output;
extern char *mock_log_buf ;
extern size_t mock_log_len;

extern const char *reset_color;

const char *log_color(int color);


/* TODO: global logging limit mask */

#define log(where, level, fmt, args...) do {				\
	if (mock_output) {						\
		char tmp[4096]			;			\
		int len = snprintf(tmp, sizeof(tmp),			\
				   "\t" fmt, ##args);			\
		size_t new_len = mock_log_len + len;			\
		mock_log_buf = realloc(mock_log_buf, new_len + 1);	\
		assert(mock_log_buf);					\
		strcpy(mock_log_buf + mock_log_len, tmp);		\
                mock_log_len = new_len;					\
	} else {							\
		if ((where) & SYSLOG)					\
			syslog(level, fmt, ##args);			\
		if ((where) & TERM) {					\
			fputs(log_color(level), stderr);		\
			fprintf(stderr, "%s: ", binary_name);		\
			fprintf(stderr, fmt, ##args);			\
			fputs(reset_color, stderr);			\
			fflush(stderr);					\
		}							\
	}								\
} while (0)

/* Ancillary routines to output logs when mock_output is true */

void ras_logger_clean(void);
void ras_logger_flush(void);

#endif
