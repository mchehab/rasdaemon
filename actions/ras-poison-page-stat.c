// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * Copyright (C) 2025 Alibaba Inc
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syslog.h>

#include "core/modules.h"
#include "core/ras-events.h"
#include "core/ras-logger.h"
#include "core/types.h"
#include "actions/ras-poison-page-stat.h"

#define POISON_STAT_THRESHOLD "POISON_STAT_THRESHOLD"

static unsigned long long poison_stat_threshold;
#ifdef HAVE_UNITTEST
static unsigned int poison_stat_calls;
#endif

int ras_poison_page_stat(void)
{
	FILE *fp;
	char line[MAX_PATH];
	unsigned long long corrupted_kb = 0;

#ifdef HAVE_UNITTEST
	poison_stat_calls++;
#endif
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

#ifdef HAVE_UNITTEST
unsigned int ras_poison_page_stat_test_calls(void)
{
	return poison_stat_calls;
}
#endif

static int poison_page_stat_consume(struct ras_events *ras, int event,
				    void *data)
{
	ras_poison_page_stat();
	return 0;
}

static const struct ras_event_consumer poison_page_stat_consumer = {
	.name = "poison-page-stat",
	.priority = PRI_POISON_PAGE,
	.events = BIT_ULL(MF_EVENT),
	.consume = poison_page_stat_consume,
};

static int poison_page_stat_init(struct ras_module_ctx *ctx)
{
	const char *value = getenv(POISON_STAT_THRESHOLD);

	poison_stat_threshold = value ? strtoull(value, NULL, 0) : 0;
	if (poison_stat_threshold)
		log(TERM, LOG_INFO,
		    "Threshold of poison page statistics is %lld kB\n",
		    poison_stat_threshold);
#ifdef HAVE_UNITTEST
	poison_stat_calls = 0;
#endif

	return ras_event_consumer_register(&poison_page_stat_consumer);
}

static void poison_page_stat_cleanup(struct ras_module_ctx *ctx)
{
	ras_event_consumer_unregister(&poison_page_stat_consumer);
}

static const struct ras_module_entry poison_page_stat_module = {
	.name = "poison-page-stat",
	.level = ACTIONS_MODULE,
	.init = poison_page_stat_init,
	.cleanup = poison_page_stat_cleanup,
};

REGISTER_RAS_MODULE(poison_page_stat_module);
