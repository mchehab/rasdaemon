/* SPDX-License-Identifier: GPL-2.0-or-later */

/*
 * Copyright (C) 2013 Petr Holasek <pholasek@redhat.com>
 */

#include <stdbool.h>

#include "core/ras-logger.h"

bool mock_output = false;

char *mock_log_buf = NULL;
size_t mock_log_len = 0;

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
