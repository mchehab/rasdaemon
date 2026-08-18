/* SPDX-License-Identifier: GPL-2.0-or-later */

/*
 * Copyright (C) 2013 Petr Holasek <pholasek@redhat.com>
 */

#include <stdbool.h>
#include <unistd.h>

#include "core/ras-logger.h"

static bool stdout_is_vt = false;

bool mock_output = false;

char *mock_log_buf = NULL;
size_t mock_log_len = 0;

const char *reset_color = "";

void ras_logger_clean(void)
{
	free(mock_log_buf);
	mock_log_buf = NULL;
	mock_log_len = 0;
}

void ras_logger_flush(void)
{
	fputs(mock_log_buf, stderr);
	ras_logger_clean();
}

enum ansi_color {
	GREEN,
	RED,
	YELLOW,
	RESET,

	ANSI_MAX_COLORS
};

static const char *const codes[] = {
	[LOG_EMERG]    = "\033[1;37;41m",
	[LOG_ALERT]    = "\033[31;47m",
	[LOG_CRIT]     = "\033[33;47m",
	[LOG_ERR]      = "\033[31;1m",
	[LOG_WARNING]  = "\033[36;1m",
	[LOG_NOTICE]   = "\033[37;1m",
	[LOG_INFO]     = "\033[37m",
	[LOG_DEBUG]    = "\033[0m",
};

const char *log_color(int color)
{
	if (!stdout_is_vt || color > LOG_DEBUG)
		return "";

	return codes[color];
}

__attribute__((constructor(200))) void ras_logger_init(void)
{
	stdout_is_vt = isatty(fileno(stdout));

	reset_color = log_color(LOG_DEBUG);
}
