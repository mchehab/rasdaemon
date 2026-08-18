// SPDX-License-Identifier: GPL-2.0

/*
 * Copyright (C) 2026 Mauro Carvalho Chehab <mchehab+huawei@kernel.org>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include "ras-logger.h"

/* Maximum number of environment variables to load */

/* Maximum line length */
#define MAX_LINE_LEN 1024

/**
 * Trim leading and trailing whitespace in-place.
 */
static void ras_trim(char *s)
{
	char *start = s;
	while (*start == ' ' || *start == '\t')
		start++;
	if (start != s)
		memmove(s, start, strlen(start) + 1);

	char *end = s + strlen(s) - 1;
	while (end > s && (*end == ' ' || *end == '\t'))
		*end-- = '\0';
}

/**
 * Parse a configuration file and set environment variables.
 *
 * Format:
 *   KEY=value
 *
 * Empty lines and lines starting with '#' or ';' are ignored.
 *
 * Spaces and tabs are allowed.
 *
 * @param fname   Path to the configuration file.
 * @return 0 on success, -1 on error.
 */
int ras_set_env(const char *fname)
{
	char line[MAX_LINE_LEN], *key, *value, *p;
	FILE *fp = NULL;
	int nenv = 0;
	int ln = 1;

	fp = fopen(fname, "r");
	if (!fp) {
		log(TERM, LOG_ERR, "Failed to open config file %s: %s\n",
		    fname, strerror(errno));
		return -1;
	}

	while (fgets(line, sizeof(line), fp)) {
		ras_trim(line);

		if (!*line || line[0] == '#' || line[0] == ';') {
			ln++;
			continue;
		}

		key = strtok(line, "=");
		if (!key) {
			ln++;
			continue;
		}

		value = strtok(NULL, "\n");
		if (!value) {
			ln++;
			continue;
		}

		ras_trim(key);
		ras_trim(value);

		/* Validate key is not empty after trimming */
		if (!*key) {
			log(TERM, LOG_ERR,
			    "line %d: Empty key in config line\n", ln);
			fclose(fp);
			return -1;
		}

		if (setenv(key, value, 0) != 0) {
			p = getenv(key);
			if (p) {
				log(TERM, LOG_INFO, "Skipping %s=%s (already set to %s)\n",
				    key, value, p);
			} else {
				log(TERM, LOG_ERR,
				    "line %d: failed to set env var %s to %s: %s\n",
				    ln, key, value, strerror(errno));
			}
		} else {
			nenv++;
		}
		ln++;
	}

	fclose(fp);

	log(TERM, LOG_INFO, "Read %d config vars from %s.\n", nenv, fname);

	return 0;
}
