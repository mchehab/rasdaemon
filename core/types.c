// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * Copyright (C) 2025 Alibaba Inc
 */

#include "core/types.h"

#include <stdio.h>

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

const char *ras_uuid_str(const char *uu, enum ras_uuid_byte_order order)
{
	static char uuid[sizeof("xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx")];
	static const unsigned char le[16] = {
		3, 2, 1, 0, 5, 4, 7, 6, 8, 9, 10, 11, 12, 13, 14, 15
	};
	static const unsigned char be[16] = {
		0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
	};
	const unsigned char *map = order == RAS_UUID_LE ? le : be;
	char *p = uuid;
	int i;

	for (i = 0; i < 16; i++) {
		p += snprintf(p, sizeof(uuid) - (p - uuid), "%.2x",
			      (unsigned char)uu[map[i]]);
		switch (i) {
		case 3:
		case 5:
		case 7:
		case 9:
			*p++ = '-';
			break;
		}
	}
	*p = '\0';

	return uuid;
}
