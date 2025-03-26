// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * Copyright (C) 2025 Alibaba Inc
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syslog.h>

#include "ras-logger.h"
#include "ras-poison-page-stat.h"
#include "types.h"

unsigned long long poison_stat_threshold;
int ras_poison_page_stat(void)
{
	FILE *fp;
	char line[MAX_PATH];
	unsigned long long corrupted_kb = 0;

	fp = fopen("/proc/meminfo", "r");
	if (!fp) {
		log(ALL, LOG_ERR, "Failed to open /proc/meminfo");
		return EXIT_FAILURE;
	}

	while (fgets(line, sizeof(line), fp))
		if (strstr(line, "HardwareCorrupted"))
			if (sscanf(line, "%*s %llukB", &corrupted_kb) == 1)
				break;

	fclose(fp);

	if (corrupted_kb > poison_stat_threshold)
		log(ALL, LOG_WARNING, "Poison page statistics exceeded threshold: %lld kB (threshold: %lld kB)\n",
		    corrupted_kb, poison_stat_threshold);

	return 0;
}
