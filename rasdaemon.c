/*
 * Copyright (C) 2013 Mauro Carvalho Chehab <mchehab+redhat@kernel.org>
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
#define DISABLE "DISABLE"
char *choices_disable;

const char *argp_program_version = TOOL_NAME " " VERSION;
const char *argp_program_bug_address = "Mauro Carvalho Chehab <mchehab@kernel.org>";

struct arguments {
	int record_events;
	int enable_ras;
	int foreground;
	int offline;
};

enum OFFLINE_ARG_KEYS {
	SMCA = 0x100,
	MODEL,
	FAMILY,
	BANK_NUM,
	IPID_REG,
	STATUS_REG,
	SYNDROME_REG
};

struct ras_mc_offline_event event;

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
#ifdef HAVE_MCE
	case 'p':
		if (state->argc < 4)
			argp_state_help(state, stdout, ARGP_HELP_LONG | ARGP_HELP_EXIT_ERR);
		args->offline++;
		break;
#endif
	default:
		return ARGP_ERR_UNKNOWN;
	}
	return 0;
}

#ifdef HAVE_MCE
static error_t parse_opt_offline(int key, char *arg,
				 struct argp_state *state)
{
	switch (key) {
	case SMCA:
		event.smca = true;
		break;
	case MODEL:
		event.model = strtoul(state->argv[state->next], NULL, 0);
		break;
	case FAMILY:
		event.family = strtoul(state->argv[state->next], NULL, 0);
		break;
	case BANK_NUM:
		event.bank = atoi(state->argv[state->next]);
		break;
	case IPID_REG:
		event.ipid = strtoull(state->argv[state->next], NULL, 0);
		break;
	case STATUS_REG:
		event.status = strtoull(state->argv[state->next], NULL, 0);
		break;
	case SYNDROME_REG:
		event.synd = strtoull(state->argv[state->next], NULL, 0);
		break;
	default:
		return ARGP_ERR_UNKNOWN;
	}
	return 0;
}
#endif

long user_hz;

int main(int argc, char *argv[])
{
	struct arguments args;
	int idx = -1;

	choices_disable = getenv(DISABLE);

#ifdef HAVE_MCE
	const struct argp_option offline_options[] = {
		{"smca", SMCA, 0, 0, "AMD SMCA Error Decoding"},
		{"model", MODEL, 0, 0, "CPU Model"},
		{"family", FAMILY, 0, 0, "CPU Family"},
		{"bank", BANK_NUM, 0, 0, "Bank Number"},
		{"ipid", IPID_REG, 0, 0, "IPID Register (for SMCA systems only)"},
		{"status", STATUS_REG, 0, 0, "Status Register"},
		{"synd", SYNDROME_REG, 0, 0, "Syndrome Register"},
		{0, 0, 0, 0, 0, 0},
	};

	struct argp offline_argp = {
		.options = offline_options,
		.parser = parse_opt_offline,
		.doc = TOOL_DESCRIPTION,
		.args_doc = ARGS_DOC,
	};

	struct argp_child offline_parser[] = {
		{&offline_argp, 0, "Post-Processing Options:", 0},
		{0, 0, 0, 0},
	};
#endif

	const struct argp_option options[] = {
		{"enable",  'e', 0, 0, "enable RAS events and exit", 0},
		{"disable", 'd', 0, 0, "disable RAS events and exit", 0},
#ifdef HAVE_SQLITE3
		{"record",  'r', 0, 0, "record events via sqlite3", 0},
#endif
		{"foreground", 'f', 0, 0, "run foreground, not daemonize"},
#ifdef HAVE_MCE
		{"post-processing", 'p', 0, 0,
		"Post-processing MCE's with raw register values"},
#endif

		{ 0, 0, 0, 0, 0, 0 }
	};
	const struct argp argp = {
		.options = options,
		.parser = parse_opt,
		.doc = TOOL_DESCRIPTION,
		.args_doc = ARGS_DOC,
#ifdef HAVE_MCE
		.children = offline_parser,
#endif
	};
	memset(&args, 0, sizeof(args));

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

#ifdef HAVE_MCE
	if (args.offline) {
		ras_offline_mce_event(&event);
		return 0;
	}
#endif

	openlog(TOOL_NAME, 0, LOG_DAEMON);
	if (!args.foreground)
		if (daemon(0, 0))
			exit(EXIT_FAILURE);

	handle_ras_events(args.record_events);

	return 0;
}
