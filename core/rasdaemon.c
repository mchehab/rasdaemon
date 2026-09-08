// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * Copyright (C) 2013 Mauro Carvalho Chehab <mchehab+huawei@kernel.org>
 */

#include <argp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "core/modules.h"
#include "core/ras-env.h"
#include "core/ras-events.h"
#include "core/ras-logger.h"
#include "core/types.h"
#include "db/ras-db.h"
#include "events/ras-mc-handler.h"

/*
 * Arguments(argp) handling logic and main
 */

#define PROG_NAME "rasdaemon"
#define TOOL_DESCRIPTION "RAS daemon to log the RAS events."
#define ARGS_DOC "<options>"
#define DISABLE "DISABLE"
#define MC_CE_STAT_THRESHOLD "MC_CE_STAT_THRESHOLD"

/**
 * DOC: rasdaemon_conf
 *
 * ``rasdaemon_conf`` is the static default configuration file path.
 */
static const char *const rasdaemon_conf = RASDAEMON_ENV;

/**
 * var argp_program_version - version string exposed by argp
 */
const char *argp_program_version = PROG_NAME " " VERSION;
/**
 * var argp_program_bug_address - maintainer address exposed by argp
 */
const char *argp_program_bug_address = "Mauro Carvalho Chehab <mchehab@kernel.org>";

/**
 * struct arguments - parsed command-line state
 * @record_events: database recording request count
 * @enable_ras: positive to enable or negative to disable tracing
 * @foreground: nonzero to avoid daemonizing
 * @cfg_file: explicit environment configuration path
 */
struct arguments {
	int record_events;
	int enable_ras;
	int foreground;
	char *cfg_file;
};

/**
 * parse_opt - parse top-level rasdaemon options
 * @k: argp option key
 * @arg: optional argument text
 * @state: argp parser state containing struct arguments
 *
 * Return:
 * * 0 - the option was handled
 * * @ARGP_ERR_UNKNOWN - @k is not a top-level option
 */
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

	case 'c':
		args->cfg_file = arg;
		return 0;

	case 'r':
		args->record_events++;
		break;
	case 'f':
		args->foreground++;
		break;
	default:
		return ARGP_ERR_UNKNOWN;
	}
	return 0;
}

/**
 * main - rasdaemon process entry point
 * @argc: argument count
 * @argv: argument vector
 *
 * Initializes modules, tracing, and the optional database session in ownership
 * order. Cleanup reverses that order after event handling ends.
 *
 * Return:
 * * @EXIT_SUCCESS - the requested operation completed
 * * @EXIT_FAILURE - initialization or database setup/cleanup failed
 * * -1 - argp did not produce a valid post-parse argument index
 * * ``-errno`` - the main RAS context could not be allocated
 */
int main(int argc, char *argv[])
{
	struct ras_events *ras;
	struct arguments args;
	struct argp_child *module_children;
	int idx = -1;
	int rc = EXIT_SUCCESS;

	/* Handle arguments before anything else */

	const struct argp_option options[] = {
		{"enable",     'e', 0,       0, "enable RAS events and exit", 0},
		{"disable",    'd', 0,       0, "disable RAS events and exit", 0},
		{"config",     'c', "FNAME", 0, "config file with env vars", 0},
		{"foreground", 'f', 0,       0, "run foreground, not daemonize", 0},

		{"record",     'r', 0,       0, "record events at the SQL backend", 0},

		{ 0, 0, 0, 0, 0, 0 }
	};
	if (modules_argp_children(&module_children))
		return EXIT_FAILURE;

	const struct argp argp = {
		.options = options,
		.parser = parse_opt,
		.doc = TOOL_DESCRIPTION,
		.args_doc = ARGS_DOC,
		.children = module_children,
	};

	memset(&args, 0, sizeof(args));
	argp_parse(&argp, argc, argv, 0,  &idx, &args);
	if (idx < 0) {
		argp_help(&argp, stderr, ARGP_HELP_STD_HELP, PROG_NAME);
		modules_argp_children_free(module_children);
		return -1;
	}
	modules_argp_children_free(module_children);

	/* Now that arguments were parsed and it is not help, proceed */

	user_hz = sysconf(_SC_CLK_TCK);

	if (args.cfg_file)
		ras_set_env(args.cfg_file);
	else
		ras_set_env(rasdaemon_conf);

	choices_disable = getenv(DISABLE);

	if (getenv(MC_CE_STAT_THRESHOLD))
		mc_ce_stat_threshold = strtoull(getenv(MC_CE_STAT_THRESHOLD), NULL, 0);
	if (mc_ce_stat_threshold)
		log(TERM, LOG_INFO, "Threshold of memory Corrected Errors statistics is %lld\n", mc_ce_stat_threshold);

	if (args.enable_ras) {
		int enable;

		enable = (args.enable_ras > 0) ? 1 : 0;
		toggle_ras_mc_event(enable);

		return 0;
	}

	rc = modules_argp_dispatch();
	if (rc >= 0)
		return rc;

	openlog(PROG_NAME, 0, LOG_DAEMON);
	if (!args.foreground)
		if (daemon(0, 0))
			exit(EXIT_FAILURE);

	ras = calloc(1, sizeof(*ras));
	if (!ras) {
		log(TERM, LOG_ERR, "Can't allocate memory for ras struct\n");
		return -errno;
	}

	if (modules_init(ras)) {
		modules_unregister();
		free(ras);
		return EXIT_FAILURE;
	}

	db_backend_enable(NULL);

	if (ras_events_prepare(ras, args.record_events)) {
		modules_unregister();
		free(ras);
		return EXIT_FAILURE;
	}

	if (args.record_events && db_open(NULL, 0, ras, 0)) {
		log(TERM, LOG_ERR, "Failed to open SQL database\n");
		ras_events_cleanup(ras);
		modules_unregister();
		free(ras);
		return EXIT_FAILURE;
	}

	rc = handle_ras_events(ras);
	if (args.record_events && db_close(0, ras)) {
		log(TERM, LOG_ERR, "Failed to close SQL database\n");
		rc = EXIT_FAILURE;
	}
	modules_unregister();
	free(ras);

	if (rc)
		return EXIT_FAILURE;

	return EXIT_SUCCESS;
}
