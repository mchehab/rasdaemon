/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2013 Petr Holasek <pholasek@redhat.com>
 */

#include <stdbool.h>
#include <unistd.h>

#include "core/ras-logger.h"

/**
 * var stdout_is_vt - whether standard output is attached to a terminal
 */
static bool stdout_is_vt = false;

/**
 * var mock_output - redirect log output to the in-memory test buffer
 */
bool mock_output = false;

/**
 * var mock_log_buf - dynamically allocated test log buffer
 */
char *mock_log_buf = NULL;
/**
 * var mock_log_len - number of bytes stored in mock_log_buf
 */
size_t mock_log_len = 0;

/**
 * var reset_color - terminal escape used to reset log colors
 */
const char *reset_color = "";

/**
 * ras_logger_clean - release the in-memory test log buffer
 */
void ras_logger_clean(void)
{
	free(mock_log_buf);
	mock_log_buf = NULL;
	mock_log_len = 0;
}

/**
 * ras_logger_flush - write and release the in-memory test log buffer
 */
void ras_logger_flush(void)
{
	fputs(mock_log_buf, stderr);
	ras_logger_clean();
}

/**
 * enum ansi_color - symbolic terminal colors
 * @GREEN: green text
 * @RED: red text
 * @YELLOW: yellow text
 * @RESET: terminal default
 * @ANSI_MAX_COLORS: number of symbolic colors
 */
enum ansi_color {
	GREEN,
	RED,
	YELLOW,
	RESET,

	ANSI_MAX_COLORS
};

/**
 * DOC: codes
 *
 * ``codes`` is the static table of terminal escapes indexed by syslog
 * severity.
 */
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

/**
 * log_color - obtain the terminal escape for a log severity
 * @color: syslog severity
 *
 * Return:
 * an escape sequence when standard output is a terminal and @color is a valid
 * syslog severity up to @LOG_DEBUG; otherwise an empty string.
 */
const char *log_color(int color)
{
	if (!stdout_is_vt || color > LOG_DEBUG)
		return "";

	return codes[color];
}

/**
 * DOC: ras_logger_init
 *
 * ``ras_logger_init()`` is the static constructor which initializes terminal
 * color state before main().
 */
static void __attribute__((constructor(200))) ras_logger_init(void)
{
	stdout_is_vt = isatty(fileno(stdout));

	reset_color = log_color(LOG_DEBUG);
}
