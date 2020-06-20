/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2020-2020. All rights reserved.
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
*/

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "ras-logger.h"
#include "ras-page-isolation.h"

#define PARSED_ENV_LEN 50
static const struct config threshold_units[] = {
	{ "m",	1000 },
	{ "k",	1000 },
	{ "",	1    },
	{}
};

static const struct config cycle_units[] = {
	{ "d",	24 },
	{ "h",	60 },
	{ "m",	60 },
	{ "s",  1  },
	{}
};

static struct isolation threshold = {
	.name = "PAGE_CE_THRESHOLD",
	.units = threshold_units,
	.env = "50",
	.unit = "",
};

static struct isolation cycle = {
	.name = "PAGE_CE_REFRESH_CYCLE",
	.units = cycle_units,
	.env = "24h",
	.unit = "h",
};

static const char *kernel_offline[] = {
	[OFFLINE_SOFT]		 = "/sys/devices/system/memory/soft_offline_page",
	[OFFLINE_HARD]		 = "/sys/devices/system/memory/hard_offline_page",
	[OFFLINE_SOFT_THEN_HARD] = "/sys/devices/system/memory/soft_offline_page",
};

static const struct config offline_choice[] = {
	{ "off",		OFFLINE_OFF },
	{ "account",		OFFLINE_ACCOUNT },
	{ "soft",		OFFLINE_SOFT },
	{ "hard",		OFFLINE_HARD },
	{ "soft-then-hard",	OFFLINE_SOFT_THEN_HARD },
	{}
};

static const char *page_state[] = {
	[PAGE_ONLINE]		= "online",
	[PAGE_OFFLINE]		= "offlined",
	[PAGE_OFFLINE_FAILED]	= "offline-failed",
};

static enum otype offline = OFFLINE_SOFT;
static struct rb_root page_records;

static void page_offline_init(void)
{
	const char *env = "PAGE_CE_ACTION";
	char *choice = getenv(env);
	const struct config *c = NULL;
	int matched = 0;

	if (choice) {
		for (c = offline_choice; c->name; c++) {
			if (!strcasecmp(choice, c->name)) {
				offline = c->val;
				matched = 1;
				break;
			}
		}
	}

	if (!matched)
		log(TERM, LOG_INFO, "Improper %s, set to default soft\n", env);

	if (offline > OFFLINE_ACCOUNT && access(kernel_offline[offline], W_OK)) {
		log(TERM, LOG_INFO, "Kernel does not support page offline interface\n");
		offline = OFFLINE_ACCOUNT;
	}

	log(TERM, LOG_INFO, "Page offline choice on Corrected Errors is %s\n",
	    offline_choice[offline].name);
}

static void parse_isolation_env(struct isolation *config)
{
	char *env = getenv(config->name);
	char *unit = NULL;
	const struct config *units = NULL;
	int i, no_unit;
	int valid = 0;
	int unit_matched = 0;
	unsigned long value, tmp;

	/* check if env is vaild */
	if (env && strlen(env)) {
		/* All the character before unit must be digit */
		for (i = 0; i < strlen(env) - 1; i++) {
			if (!isdigit(env[i]))
				goto parse;
		}
		if (sscanf(env, "%lu", &value) < 1 || !value)
			goto parse;
		/* check if the unit is vaild */
		unit = env + strlen(env) - 1;
		/* no unit, all the character are value character */
		if (isdigit(*unit)) {
			valid = 1;
			no_unit = 1;
			goto parse;
		}
		for (units = config->units; units->name; units++) {
			/* value character and unit character are both valid */
			if (!strcasecmp(unit, units->name)) {
				valid = 1;
				no_unit = 0;
				break;
			}
		}
	}

parse:
	/* if invalid, use default env */
	if (valid) {
		config->env = env;
		if (!no_unit)
			config->unit = unit;
	} else {
		 log(TERM, LOG_INFO, "Improper %s, set to default %s.\n",
				 config->name, config->env);
	}

	/* if env value string is greater than ulong_max, truncate the last digit */
	sscanf(config->env, "%lu", &value);
	for (units = config->units; units->name; units++) {
		if (!strcasecmp(config->unit, units->name))
			unit_matched = 1;
		if (unit_matched) {
			tmp = value;
			value *= units->val;
			if (tmp != 0 && value / tmp != units->val)
				config->overflow = true;
		}
	}
	config->val = value;
	/* In order to output value and unit perfectly */
	config->unit = no_unit ? config->unit : "";
}

static void parse_env_string(struct isolation *config, char *str)
{
	int i;

	if (config->overflow) {
		/* when overflow, use basic unit */
		for (i = 0; config->units[i].name; i++) ;
		sprintf(str, "%lu%s", config->val, config->units[i-1].name);
		log(TERM, LOG_INFO, "%s is set overflow(%s), truncate it\n",
				config->name, config->env);
	} else {
		sprintf(str, "%s%s", config->env, config->unit);
	}
}

static void page_isolation_init(void)
{
	char threshold_string[PARSED_ENV_LEN];
	char cycle_string[PARSED_ENV_LEN];
	/**
	 * It's unnecessary to parse threshold configuration when offline
	 * choice is off.
	 */
	if (offline == OFFLINE_OFF)
		return;

	parse_isolation_env(&threshold);
	parse_isolation_env(&cycle);
	parse_env_string(&threshold, threshold_string);
	parse_env_string(&cycle, cycle_string);
	log(TERM, LOG_INFO, "Threshold of memory Corrected Errors is %s / %s\n",
			threshold_string, cycle_string);
}

void ras_page_account_init(void)
{
	page_offline_init();
	page_isolation_init();
}

static int do_page_offline(unsigned long long addr, enum otype type)
{
	FILE *offline_file;
	int err;

	offline_file = fopen(kernel_offline[type], "w");
	if (!offline_file)
		return -1;

	fprintf(offline_file, "%#llx", addr);
	err = ferror(offline_file) ? -1 : 0;
	fclose(offline_file);

	return err;
}

static void page_offline(struct page_record *pr)
{
	unsigned long long addr = pr->addr;
	int ret;

	/* Offlining page is not required */
	if (offline <= OFFLINE_ACCOUNT)
		return;

	/* Ignore offlined pages */
	if (pr->offlined != PAGE_ONLINE)
		return;

	/* Time to silence this noisy page */
	if (offline == OFFLINE_SOFT_THEN_HARD) {
		ret = do_page_offline(addr, OFFLINE_SOFT);
		if (ret < 0)
			ret = do_page_offline(addr, OFFLINE_HARD);
	} else {
		ret = do_page_offline(addr, offline);
	}

	pr->offlined = ret < 0 ? PAGE_OFFLINE_FAILED : PAGE_OFFLINE;

	log(TERM, LOG_INFO, "Result of offlining page at %#llx: %s\n",
	    addr, page_state[pr->offlined]);
}

static void page_record(struct page_record *pr, unsigned count, time_t time)
{
	unsigned long period = time - pr->start;
	unsigned long tolerate;

	if (period >= cycle.val) {
		/**
		 * Since we don't refresh automatically, it is possible that the period
		 * between two occurences will be longer than the pre-configured refresh cycle.
		 * In this case, we tolerate the frequency of the whole period up to
		 * the pre-configured threshold.
		 */
		tolerate = (period / (double)cycle.val) * threshold.val;
		pr->count -= (tolerate > pr->count) ? pr->count : tolerate;
		pr->start = time;
		pr->excess = 0;
	}

	pr->count += count;
	if (pr->count >= threshold.val) {
		log(TERM, LOG_INFO, "Corrected Errors at %#llx exceeded threshold\n", pr->addr);

		/**
		 * Backup ce count of current cycle to enable next round, which actually
		 * should never happen if we can disable overflow completely in the same
		 * time unit (but sadly we can't).
		 */
		pr->excess += pr->count;
		pr->count = 0;
		page_offline(pr);
	}
}

static struct page_record *page_lookup_insert(unsigned long long addr)
{
	struct rb_node **entry = &page_records.rb_node;
	struct rb_node *parent = NULL;
	struct page_record *pr = NULL, *find = NULL;

	while (*entry) {
		parent = *entry;
		pr = rb_entry(parent, struct page_record, entry);
		if (addr == pr->addr) {
			return pr;
		} else if (addr < pr->addr) {
			entry = &(*entry)->rb_left;
		} else {
			entry = &(*entry)->rb_right;
		}
	}

	find = calloc(1, sizeof(struct page_record));
	if (!find) {
		log(TERM, LOG_ERR, "No memory for page records\n");
		return NULL;
	}

	find->addr = addr;
	rb_link_node(&find->entry, parent, entry);
	rb_insert_color(&find->entry, &page_records);

	return find;
}

void ras_record_page_error(unsigned long long addr, unsigned count, time_t time)
{
	struct page_record *pr = NULL;

	if (offline == OFFLINE_OFF)
		return;

	pr = page_lookup_insert(addr & PAGE_MASK);
	if (pr) {
		if (!pr->start)
			pr->start = time;
		page_record(pr, count, time);
	}
}
