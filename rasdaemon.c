/*
 * Copyright (C) 2013 Mauro Carvalho Chehab <mchehab@redhat.com>
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

#include <argp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ras-record.h"
#include "ras-logger.h"
#include "ras-events.h"

/*
 * Arguments(argp) handling logic and main
 */

#define TOOL_NAME "rasdaemon"
#define TOOL_DESCRIPTION "RAS daemon to log the RAS events."
#define ARGS_DOC "<options>"

const char *argp_program_version = TOOL_NAME " " VERSION;
const char *argp_program_bug_address = "Mauro Carvalho Chehab <mchehab@kernel.org>";

struct arguments {
	int record_events;
	int enable_ras;
	int foreground;
};

static error_t parse_opt(int k, char *arg, struct argp_state *state)
{
	struct arguments *args = state->input;

	switch (k) {
	case 'e':
		args->enable_ras++;
		break;
	case 'd':
		args->enable_ras--;
		break;
#ifdef HAVE_SQLITE3
	case 'r':
		args->record_events++;
		break;
#endif
	case 'f':
		args->foreground++;
		break;
	default:
		return ARGP_ERR_UNKNOWN;
	}
	return 0;
}

long user_hz;

int main(int argc, char *argv[])
{
	struct arguments args;
	int idx = -1;
	const struct argp_option options[] = {
		{"enable",  'e', 0, 0, "enable RAS events and exit", 0},
		{"disable", 'd', 0, 0, "disable RAS events and exit", 0},
#ifdef HAVE_SQLITE3
		{"record",  'r', 0, 0, "record events via sqlite3", 0},
#endif
		{"foreground", 'f', 0, 0, "run foreground, not daemonize"},

		{ 0, 0, 0, 0, 0, 0 }
	};
	const struct argp argp = {
		.options = options,
		.parser = parse_opt,
		.doc = TOOL_DESCRIPTION,
		.args_doc = ARGS_DOC,

	};
	memset (&args, 0, sizeof(args));

	user_hz = sysconf(_SC_CLK_TCK);

	argp_parse(&argp, argc, argv, 0,  &idx, &args);

	if (idx < 0) {
		argp_help(&argp, stderr, ARGP_HELP_STD_HELP, TOOL_NAME);
		return -1;
	}

	if (args.enable_ras) {
		int enable;

		enable = (args.enable_ras > 0) ? 1 : 0;
		toggle_ras_mc_event(enable);

		return 0;
	}

	openlog(TOOL_NAME, 0, LOG_DAEMON);
	if (!args.foreground)
		if (daemon(0,0))
			exit(EXIT_FAILURE);

	handle_ras_events(args.record_events);

	return 0;
}
