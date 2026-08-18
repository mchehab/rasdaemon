// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * Copyright (C) 2013 Mauro Carvalho Chehab <mchehab+huawei@kernel.org>
 */

#include <argp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "core/types.h"
#include "core/ras-env.h"
#include "core/ras-events.h"
#include "core/ras-logger.h"
#include "core/ras-mc-handler.h"
#include "core/modules.h"

#include "db/ras-db.h"
#include "db/ras-record.h"

#include "events/ras-poison-page-stat.h"

#include "events-arch-x86/ras-erst.h"

/*
 * Arguments(argp) handling logic and main
 */

#define PROG_NAME "rasdaemon"
#define TOOL_DESCRIPTION "RAS daemon to log the RAS events."
#define ARGS_DOC "<options>"
#define DISABLE "DISABLE"
#define MC_CE_STAT_THRESHOLD "MC_CE_STAT_THRESHOLD"
#define POISON_STAT_THRESHOLD "POISON_STAT_THRESHOLD"

static const char *const rasdaemon_conf = RASDAEMON_ENV;

const char *argp_program_version = PROG_NAME " " VERSION;
const char *argp_program_bug_address = "Mauro Carvalho Chehab <mchehab@kernel.org>";

struct arguments {
	int record_events;
	int enable_ras;
	int enable_ipmitool;
	int foreground;
	int offline;
	char *cfg_file;
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

	case 'c':
		args->cfg_file = arg;
		return 0;

#ifdef HAVE_DB
	case 'r':
		args->record_events++;
		break;
#endif
#ifdef HAVE_OPENBMC_UNIFIED_SEL
	case 'i':
		args->enable_ipmitool++;
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
		event.model = strtoul(arg, NULL, 0);
		break;
	case FAMILY:
		event.family = strtoul(arg, NULL, 0);
		break;
	case BANK_NUM:
		event.bank = atoi(arg);
		break;
	case IPID_REG:
		event.ipid = strtoull(arg, NULL, 0);
		break;
	case STATUS_REG:
		event.status = strtoull(arg, NULL, 0);
		break;
	case SYNDROME_REG:
		event.synd = strtoull(arg, NULL, 0);
		break;
	default:
		return ARGP_ERR_UNKNOWN;
	}
	return 0;
}
#endif

int main(int argc, char *argv[])
{
	struct ras_events *ras;
	struct arguments args;
	int idx = -1;

	/* Handle arguments before anything else */

#ifdef HAVE_MCE
	const struct argp_option offline_options[] = {
		{"smca", SMCA, 0, 0, "AMD SMCA Error Decoding"},
		{"model", MODEL, "MODEL", 0, "CPU Model"},
		{"family", FAMILY, "FAMILY", 0, "CPU Family"},
		{"bank", BANK_NUM, "BANK_NUM", 0, "Bank Number"},
		{"ipid", IPID_REG, "IPID_REG", 0, "IPID Register (for SMCA systems only)"},
		{"status", STATUS_REG, "STATUS_REG", 0, "Status Register"},
		{"synd", SYNDROME_REG, "SYNDROME_REG", 0, "Syndrome Register"},
		{0, 0, 0, 0, 0, 0},
	};

	struct argp offline_argp = {
		.options = offline_options,
		.parser = parse_opt_offline,
		.doc = TOOL_DESCRIPTION,
		.args_doc = ARGS_DOC,
	};

	struct argp_child offline_parser[] = {
		{&offline_argp, 0, "MCE Post-Processing Options:", 1},
		{0, 0, 0, 0},
	};
#endif

	const struct argp_option options[] = {
		{"enable",     'e', 0,       0, "enable RAS events and exit", 0},
		{"disable",    'd', 0,       0, "disable RAS events and exit", 0},
		{"config",     'c', "FNAME", 0, "config file with env vars", 0},
		{"foreground", 'f', 0,       0, "run foreground, not daemonize", 0},

#ifdef HAVE_DB
		{"record",     'r', 0,       0, "record events at the SQL backend", 0},
#endif
#ifdef HAVE_OPENBMC_UNIFIED_SEL
		{"ipmitool",   'i', 0,       0, "enable ipmitool logging", 0},
#endif
#ifdef HAVE_MCE
		{"post-processing", 'p', 0, 0,
		"Post-processing MCE's with raw register values", 2},
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
	argp_parse(&argp, argc, argv, 0,  &idx, &args);
	if (idx < 0) {
		argp_help(&argp, stderr, ARGP_HELP_STD_HELP, PROG_NAME);
		return -1;
	}

	/* Now that arguments were parsed and it is not help, proceed */

	user_hz = sysconf(_SC_CLK_TCK);

	ras_set_env(rasdaemon_conf);

	choices_disable = getenv(DISABLE);

	if (getenv(MC_CE_STAT_THRESHOLD))
		mc_ce_stat_threshold = strtoull(getenv(MC_CE_STAT_THRESHOLD), NULL, 0);
	if (mc_ce_stat_threshold)
		log(TERM, LOG_INFO, "Threshold of memory Corrected Errors statistics is %lld\n", mc_ce_stat_threshold);

#ifdef HAVE_POISON_PAGE_STAT
	if (getenv(POISON_STAT_THRESHOLD))
		poison_stat_threshold = strtoull(getenv(POISON_STAT_THRESHOLD), NULL, 0);
	if (poison_stat_threshold)
		log(TERM, LOG_INFO, "Threshold of poison page statistics is %lld kB\n", poison_stat_threshold);
#endif

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

	openlog(PROG_NAME, 0, LOG_DAEMON);
	if (!args.foreground)
		if (daemon(0, 0))
			exit(EXIT_FAILURE);

#ifdef HAVE_ERST
#ifdef HAVE_MCE
	if (choices_disable && strlen(choices_disable) != 0 &&
	    strstr(choices_disable, "ras:erst"))
		log(ALL, LOG_INFO, "Disabled ras:erst from config\n");
	else
		handle_erst();
#endif
#endif

	ras = calloc(1, sizeof(*ras));
	if (!ras) {
		log(TERM, LOG_ERR, "Can't allocate memory for ras struct\n");
		return -errno;
	}

	modules_init(ras);

	db_backend_enable(NULL);

	handle_ras_events(ras, args.record_events, args.enable_ipmitool);

	return 0;
}
