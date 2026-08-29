// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * Copyright (C) 2025 Alibaba Inc
 */

#include "core/types.h"

/**
 * var loglevel_str - display labels indexed by rasdaemon log severity
 */
const char *loglevel_str[] = {
	[LOGLEVEL_EMERG]	= "[EMERG]",
	[LOGLEVEL_ALERT]	= "[ALERT]",
	[LOGLEVEL_CRIT]		= "[CRIT]",
	[LOGLEVEL_ERR]		= "[ERROR]",
	[LOGLEVEL_WARNING]	= "[WARNING]",
	[LOGLEVEL_NOTICE]	= "[NOTICE]",
	[LOGLEVEL_INFO]		= "[INFO]",
	[LOGLEVEL_DEBUG]	= "[DEBUG]",
};
