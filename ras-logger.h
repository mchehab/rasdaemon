/*
 * Copyright (C) 2013 Petr Holasek <pholasek@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
*/

#ifndef __RAS_LOGGER_H

#include <syslog.h>

/*
 * Logging macros
 */

#ifndef TOOL_NAME
	#define TOOL_NAME "rasdaemon"
#endif

#define SYSLOG	(1 << 0)
#define TERM	(1 << 1)
#define ALL	(SYSLOG | TERM)
/* TODO: global logging limit mask */

#define log(where, level, fmt, args...) do {\
	if (where & SYSLOG)\
		syslog(level, fmt, ##args);\
	if (where & TERM) {\
		fprintf(stderr, "%s: ", TOOL_NAME);\
		fprintf(stderr, fmt, ##args);\
		fflush(stderr);\
	}\
} while (0)

#define __RAS_LOGGER_H
#endif
