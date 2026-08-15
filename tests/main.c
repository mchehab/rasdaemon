// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2026 Mauro Carvalho Chehab <mchehab+huawei@kernel.org>
 */

#include <argp.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "config.h"

#include "core/ras-logger.h"
#include "tests/unittest.h"

static const struct test_group groups[] = {
#if HAVE_MYSQL > 0
	{ "mysql", test_mysql },
#endif
#if HAVE_SQLITE3 > 0
	{ "sqlite3", test_sqlite3 },
#endif

	/* Should be the last one, as it will mock with probed modules */
	{ "modules", test_modules },
};

static bool stdout_is_vt = false;

enum ansi_color {
	GREEN,
	RED,
	YELLOW,
	RESET,

	ANSI_MAX_COLORS
};

static const char *const codes[] = {
	[GREEN]  = "\033[32m",
	[RED]    = "\033[31;1m",
	[YELLOW] = "\033[33;1m",
	[RESET]  = "\033[0m"
};

static const char *get_color(enum ansi_color color)
{
	if (!stdout_is_vt || color > ANSI_MAX_COLORS)
		return NULL;

	return codes[color];
}

const char *argp_program_version = "rasdaemon unit tests 0.1.0";
const char *argp_program_bug_address = "mchehab@kernel.org";

static const size_t group_count = sizeof(groups) / sizeof(groups[0]);

struct arguments {
	const char *selected_group;
	const char *test_filter;
	const char *skip_filter;
	uint32_t output_formats;
	bool output_was_set;
	bool list_groups;
};

static const struct argp_option options[] = {
	{ "output", 'o', "FORMAT", 0, "Output format; may be repeated: standard, tap, xml, subunit", 0 },
	{ "filter", 'f', "PATTERN", 0, "Run test names matching PATTERN; supports '*' and '?'", 0 },
	{ "skip", 's', "PATTERN", 0, "Skip test names matching PATTERN; supports '*' and '?'", 0 },
	{ "group", 'g', "NAME", 0, "Run only the named test group", 0 },
	{ "list-groups", 'l', NULL, 0, "List available test groups", 0 },
	{ 0 }
};

static const char doc[] ="Run rasdaemon unit tests.";

static void list_groups(FILE *stream)
{
	size_t index;

	for (index = 0; index < group_count; ++index) {
		fprintf(stream, "%s\n", groups[index].name);
	}
}

static uint32_t parse_output_format(
	const char *value,
	struct argp_state *state
)
{
	if (strcasecmp(value, "standard") == 0) {
		return CM_OUTPUT_STANDARD;
	}

	if (strcasecmp(value, "tap") == 0) {
		return CM_OUTPUT_TAP;
	}

	if (strcasecmp(value, "xml") == 0) {
		return CM_OUTPUT_XML;
	}

	if (strcasecmp(value, "subunit") == 0) {
		return CM_OUTPUT_SUBUNIT;
	}

	argp_error(state,
		   "unknown format: %s. Expected: standard, tap, xml, or subunit",
		   value);

	return 0;
}

static error_t parse_option(int key, char *value, struct argp_state *state)
{
	struct arguments *args = state->input;

	switch (key) {
		case 'o':
			args->output_formats |=	parse_output_format(value,
								    state);
			args->output_was_set = true;
			return 0;

		case 'f':
			args->test_filter = value;
			return 0;

		case 's':
			args->skip_filter = value;
			return 0;

		case 'g':
			args->selected_group = value;
			return 0;

		case 'l':
			args->list_groups = true;
			return 0;

		case ARGP_KEY_ARG:
			argp_error(state,
				   "unexpected positional value '%s'", value);
			return 0;

		case ARGP_KEY_END:
			return 0;

		default:
			return ARGP_ERR_UNKNOWN;
	}
}

static const struct argp argp = { options, parse_option, NULL, doc };

static bool group_is_selected(const struct test_group *group,
			      const char *selected_group)
{
	return selected_group == NULL || !strcasecmp(group->name, selected_group);
}

static void filter_output(const char *format, va_list args)
{
	const char *color = NULL;

	/*
	 * NOTE: This is a poor man approach, as it assumes that formats
	 *	 will use the first 12 chars for the type. A better way
	 *	 would be to use XML or TAP and parse it, but this is simple
	 *	 enough. Worse case scenario is that, if this changes, the log
	 *	 won't use colors, which is not the end of times.
	 */

	if (!(*format == '[') || strlen(format) < 12 || !(format[10] != ']')) {
		vfprintf(stdout, format, args);
		return;
	}

#if 0 /* we should add a flag to optionally drop it */
	if (!strncmp(format, "[ RUN      ]", 12)) {
		return;
	}
#endif

	if (!strncmp(format, "[       OK ]", 12) ||
            !strncmp(format, "[  PASSED  ]", 12))
		color = get_color(GREEN);
	else if (!strncmp(format, "[  FAILED  ]", 12) ||
		 !strncmp(format, "[   LINE   ]", 12) ||
		 !strncmp(format, "[  ERROR   ]", 12))
		color = get_color(RED);
	else if (!strncmp(format, "[  SKIPPED ]", 12))
		color = get_color(YELLOW);

	fputc('[', stdout);

	if (color)
		fputs(color, stdout);

	for (int i=1; i < 11; i++)
		fputc(format[i], stdout);

	color = get_color(RESET);
	if (color)
		fputs(color, stdout);

	fputc(']', stdout);

	vfprintf(stdout, &format[12], args);
}

const struct CMCallbacks callbacks = {
	.vprint_message = filter_output,
	.vprint_error = filter_output,
};

int main(int argc, char **argv)
{
	struct arguments arguments = { 0 };

	bool found_group = false;
	int failed_groups = 0;
	size_t index;

	argp_parse(&argp, argc, argv, 0, NULL, &arguments);

	if (arguments.list_groups) {
		list_groups(stdout);
		return EXIT_SUCCESS;
	}

	if (arguments.output_was_set) {
		cmocka_set_message_output(arguments.output_formats);
	}

	if (arguments.test_filter != NULL) {
		cmocka_set_test_filter(arguments.test_filter);
	}

	if (arguments.skip_filter != NULL) {
		cmocka_set_skip_filter(arguments.skip_filter);
	}

	/* Set logger to mock mode */
	mock_output = true;

	cmocka_set_callbacks(&callbacks);
	stdout_is_vt = isatty(fileno(stdout));

	for (index = 0; index < group_count; ++index) {
		const struct test_group *group = &groups[index];

		if (!group_is_selected(group, arguments.selected_group)) {
			continue;
		}

		found_group = true;

		if (group->run() != 0) {
			++failed_groups;
		}
	}

	if (!found_group) {
		fprintf(stderr, "%s: unknown test group '%s'\n", argv[0],
			arguments.selected_group);
		fprintf(stderr, "Available groups:\n");
		list_groups(stderr);
		return EXIT_FAILURE;
	}

	return failed_groups == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
