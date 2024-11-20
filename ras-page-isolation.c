// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2020-2020. All rights reserved.
 */

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "ras-logger.h"
#include "ras-page-isolation.h"

#define PARSED_ENV_LEN 50
#define ROW_ID_MAX_LEN 200
#define SAME_PAGE_IN_ROW 200

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

static struct isolation row_threshold = {
	.name = "ROW_CE_THRESHOLD",
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

static struct isolation row_cycle = {
	.name = "ROW_CE_REFRESH_CYCLE",
	.units = cycle_units,
	.env = "24h",
	.unit = "h",
};

static const char * const kernel_offline[] = {
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

static const char * const page_state[] = {
	[PAGE_ONLINE]		= "online",
	[PAGE_OFFLINE]		= "offlined",
	[PAGE_OFFLINE_FAILED]	= "offline-failed",
};

static enum otype offline = OFFLINE_SOFT;
static enum otype row_offline_action = OFFLINE_OFF;
static struct rb_root page_records;
LIST_HEAD(row_listhead, row_record) row_head;

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

	if (row_offline_action != OFFLINE_OFF) {
		log(TERM, LOG_INFO, "row threshold is open, so turn off page threshold\n");
		offline = OFFLINE_OFF;
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

	/* check if env is valid */
	if (env && strlen(env)) {
		/* All the character before unit must be digit */
		for (i = 0; i < strlen(env) - 1; i++) {
			if (!isdigit(env[i]))
				goto parse;
		}
		if (sscanf(env, "%lu", &value) < 1 || !value)
			goto parse;
		/* check if the unit is valid */
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
			/*
			 * if units->val is 1,  config->env is greater than ulong_max, so it is can strtoul
			 * if failed, the value is greater than ulong_max, set config->overflow = true
			 */
			if (units->val == 1) {
				char *endptr;

				strtoul(config->env, &endptr, 10);
				if (errno == ERANGE || *endptr != '\0')
					config->overflow = true;
			}
			unit_matched = 0;
		}
	}
	config->val = value;
	/* In order to output value and unit perfectly */
	config->unit = no_unit ? config->unit : "";
}

static void parse_env_string(struct isolation *config, char *str, unsigned int size)
{
	int i;

	if (config->overflow) {
		/* when overflow, use basic unit */
		for (i = 0; config->units[i].name; i++)
			;
		snprintf(str, size, "%lu%s", config->val, config->units[i - 1].name);
		log(TERM, LOG_INFO, "%s is set overflow(%s), truncate it\n",
		    config->name, config->env);
	} else {
		snprintf(str, size, "%s%s", config->env, config->unit);
	}
}

static void page_isolation_init(void)
{
	char threshold_string[PARSED_ENV_LEN];
	char cycle_string[PARSED_ENV_LEN];
	/*
	 * It's unnecessary to parse threshold configuration when offline
	 * choice is off.
	 */
	if (offline == OFFLINE_OFF)
		return;

	parse_isolation_env(&threshold);
	parse_isolation_env(&cycle);
	parse_env_string(&threshold, threshold_string, sizeof(threshold_string));
	parse_env_string(&cycle, cycle_string, sizeof(cycle_string));
	log(TERM, LOG_INFO, "Threshold of memory Corrected Errors is %s / %s\n",
	    threshold_string, cycle_string);
}

static void row_offline_init(void)
{
	const char *env = "ROW_CE_ACTION";
	char *choice = getenv(env);
	const struct config *c = NULL;
	int matched = 0;

	if (choice) {
		for (c = offline_choice; c->name; c++) {
			if (!strcasecmp(choice, c->name)) {
				row_offline_action = c->val;
				matched = 1;
				break;
			}
		}
	}

	if (!matched)
		log(TERM, LOG_INFO, "Improper %s, set to default off\n", env);

	if (row_offline_action > OFFLINE_ACCOUNT && access(kernel_offline[row_offline_action], W_OK)) {
		log(TERM, LOG_INFO, "Kernel does not support row offline interface\n");
		row_offline_action = OFFLINE_ACCOUNT;
	}

	log(TERM, LOG_INFO, "Row offline choice on Corrected Errors is %s\n",
	    offline_choice[row_offline_action].name);
}

static void row_isolation_init(void)
{
	char threshold_string[PARSED_ENV_LEN];
	char cycle_string[PARSED_ENV_LEN];
	/*
	 * It's unnecessary to parse threshold configuration when offline
	 * choice is off.
	 */
	if (row_offline_action == OFFLINE_OFF)
		return;

	parse_isolation_env(&row_threshold);
	parse_isolation_env(&row_cycle);
	parse_env_string(&row_threshold, threshold_string, sizeof(threshold_string));
	parse_env_string(&row_cycle, cycle_string, sizeof(cycle_string));
	log(TERM, LOG_INFO, "Threshold of memory row Corrected Errors is %s / %s\n",
	    threshold_string, cycle_string);
}

void ras_row_account_init(void)
{
	row_offline_init();
	row_isolation_init();
}

void ras_page_account_init(void)
{
	page_offline_init();
	page_isolation_init();
}

static int do_page_offline(unsigned long long addr, enum otype type)
{
	int fd, rc;
	char buf[20];

	fd = open(kernel_offline[type], O_WRONLY);
	if (fd == -1) {
		log(TERM, LOG_ERR, "[%s]:open file: %s failed\n", __func__,
		    kernel_offline[type]);
		return -1;
	}

	snprintf(buf, sizeof(buf), "%#llx", addr);
	rc = write(fd, buf, strlen(buf));
	if (rc < 0)
		log(TERM, LOG_ERR,
		    "page offline addr(%s) by %s failed, errno:%d\n",
		    buf, kernel_offline[type], errno);

	close(fd);
	return rc;
}

static void page_offline(struct page_record *pr)
{
	unsigned long long addr = pr->addr;
	int ret;

	/* Offlining page is not required */
	if (offline <= OFFLINE_ACCOUNT) {
		log(TERM, LOG_INFO, "PAGE_CE_ACTION=%s, ignore to offline page at %#llx\n",
		    offline_choice[offline].name, addr);
		return;
	}

	/* Ignore offlined pages */
	if (pr->offlined == PAGE_OFFLINE) {
		log(TERM, LOG_INFO, "page at %#llx is already offlined, ignore\n", addr);
		return;
	}

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

static void page_record(struct page_record *pr, unsigned int count, time_t time)
{
	unsigned long period = time - pr->start;
	unsigned long tolerate;

	if (period >= cycle.val) {
		/*
		 * Since we don't refresh automatically, it is possible that the period
		 * between two occurrences will be longer than the pre-configured refresh cycle.
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

		/*
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
		if (addr == pr->addr)
			return pr;
		else if (addr < pr->addr)
			entry = &(*entry)->rb_left;
		else
			entry = &(*entry)->rb_right;
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

void ras_record_page_error(unsigned long long addr, unsigned int count, time_t time)
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

void ras_hw_threshold_pageoffline(unsigned long long addr)
{
	time_t now = time(NULL);

	ras_record_page_error(addr, threshold.val, now);
}

/* memory page CE threshold policy ends */

/* memory row CE threshold policy starts */
static const struct memory_location_field apei_fields[] = {
	[APEI_NODE] = {
		.name = "node",
		.anchor_str = "node:",
		.value_base = 10
	},
	[APEI_CARD] = {
		.name = "card",
		.anchor_str = "card:",
		.value_base = 10
	},
	[APEI_MODULE] = {
		.name = "module",
		.anchor_str = "module:",
		.value_base = 10
	},
	[APEI_RANK] = {
		.name = "rank",
		.anchor_str = "rank:",
		.value_base = 10
	},
	[APEI_DEVICE] = {
		.name = "device",
		.anchor_str = "device:",
		.value_base = 10
	},
	[APEI_BANK] = {
		.name = "bank",
		.anchor_str = "bank:",
		.value_base = 10
	},
	[APEI_ROW] = {
		.name = "row",
		.anchor_str = "row:",
		.value_base = 10
	},
};

static const struct memory_location_field dsm_fields[] = {
	[DSM_ProcessorSocketId] = {
		.name = "ProcessorSocketId",
		.anchor_str = "ProcessorSocketId:",
		.value_base = 16
	},
	[DSM_MemoryControllerId] = {
		.name = "MemoryControllerId",
		.anchor_str = "MemoryControllerId:",
		.value_base = 16
	},
	[DSM_ChannelId] = {
		.name = "ChannelId",
		.anchor_str = "ChannelId:",
		.value_base = 16
	},
	[DSM_DimmSlotId] = {
		.name = "DimmSlotId",
		.anchor_str = "DimmSlotId:",
		.value_base = 16
	},
	[DSM_PhysicalRankId] = {
		.name = "PhysicalRankId",
		.anchor_str = "PhysicalRankId:",
		.value_base = 16
	},
	[DSM_ChipId] = {
		.name = "ChipId",
		.anchor_str = "ChipId:",
		.value_base = 16
	},
	[DSM_BankGroup] = {
		.name = "BankGroup",
		.anchor_str = "BankGroup:",
		.value_base = 16
	},
	[DSM_Bank] = {
		.name = "Bank",
		.anchor_str = "Bank:",
		.value_base = 16
	},
	[DSM_Row] = {
		.name = "Row",
		.anchor_str = "Row:",
		.value_base = 16
	},
};

static void row_record_get_id(struct row_record *rr,
			      char *buffer, unsigned int size)
{
	const struct memory_location_field *fields;
	int pos = 0, field_num = 0, len;

	if (!rr || !buffer)
		return;

	if (rr->type == GHES) {
		field_num = APEI_FIELD_NUM_CONST;
		fields = apei_fields;
	} else {
		field_num = DSM_FIELD_NUM_CONST;
		fields = dsm_fields;
	}
	len = snprintf(buffer + pos, size, "{");
	pos += len;
	size -= len;
	for (int idx = 0; idx < field_num; idx++) {
		if (idx == field_num - 1)
			len = snprintf(buffer + pos, size, "%s:%d",
				       fields[idx].name, rr->location_fields[idx]);
		else
			len = snprintf(buffer + pos, size, "%s:%d,",
				       fields[idx].name, rr->location_fields[idx]);

		pos += len;
		size -= len;
	}
	pos += snprintf(buffer + pos, size, "}");
	buffer[pos] = '\0';
}

static bool row_record_is_same_row(struct row_record *rr1,
				   struct row_record *rr2)
{
	if (!rr1 || !rr2 || rr1->type != rr2->type)
		return false;

	int field_num = 0;

	if (rr1->type == GHES)
		field_num = APEI_FIELD_NUM_CONST;
	else
		field_num = DSM_FIELD_NUM_CONST;

	for (int idx = 0; idx < field_num; idx++) {
		if (rr1->location_fields[idx] != rr2->location_fields[idx])
			return false;
	}
	return true;
}

static void row_record_copy(struct row_record *dst, struct row_record *src)
{
	if (!dst || !src)
		return;

	for (int i = 0; i < ROW_LOCATION_FIELDS_NUM; i++)
		dst->location_fields[i] = src->location_fields[i];
}

static int parse_value(const char *str, const char *anchor_str, int value_base, int *value)
{
	char *start, *endptr;
	int tmp;

	if (!str || !anchor_str || !value)
		return 1;

	char *pos = strstr(str, anchor_str);

	if (!pos)
		return 1;

	errno = 0;
	start = pos + strlen(anchor_str);
	tmp = (int)strtol(start, &endptr, value_base);

	if (errno != 0) {
		log(TERM, LOG_ERR, "%s error, start: %s, value_base: %d, errno: %d\n",
		    __func__, start, value_base, errno);
		return 1;
	}

	if (endptr == start) {
		log(TERM, LOG_ERR, "%s error, start: %s, value_base: %d\n",
		    __func__, start, value_base);
		return 1;
	}
	*value = tmp;
	return 0;
}

static int parse_row_info(const char *detail, struct row_record *r)
{
	const struct memory_location_field *fields = NULL;
	int field_num;

	if (!detail || !r)
		return 1;

	if (strstr(detail, "APEI location")) {
		fields = apei_fields;
		field_num = APEI_FIELD_NUM_CONST;
		r->type = GHES;
	} else if (strstr(detail,  "ProcessorSocketId:")) {
		fields = dsm_fields;
		field_num = DSM_FIELD_NUM_CONST;
		r->type = DSM;
	} else {
		return 1;
	}

	for (int idx = 0; idx < field_num; idx++) {
		if (parse_value(detail,  fields[idx].anchor_str,
				fields[idx].value_base, &r->location_fields[idx])) {
			log(TERM, LOG_INFO,
			    "Cannot parse memory row info from CE detail: %s missing\n",
			    fields[idx].name);
			return 1;
		}
	}
	return 0;
}

static void row_offline(struct row_record *rr, time_t time)
{
	int ret;
	char row_id[ROW_ID_MAX_LEN] = {0};

	if (!rr)
		return;
	row_record_get_id(rr, row_id, ROW_ID_MAX_LEN);
	/* Offlining row is not required */
	if (row_offline_action <= OFFLINE_ACCOUNT) {
		log(TERM, LOG_INFO, "ROW_CE_ACTION=%s, ignore to offline row at %s\n",
		    offline_choice[row_offline_action].name, row_id);
		return;
	}

	struct page_addr *page_info = NULL;
	// do offline
	unsigned long long addr_list[SAME_PAGE_IN_ROW];
	int addr_list_size = 0;

	LIST_FOREACH(page_info, &rr->page_head, entry) {
		/* Ignore offlined pages */
		if (page_info->offlined == PAGE_OFFLINE &&
		    addr_list_size < SAME_PAGE_IN_ROW) {
			addr_list[addr_list_size++] = page_info->addr;
			continue;
		}

		int found = 0;

		for (int i = 0; i < addr_list_size; i++) {
			if (addr_list[i] ==  page_info->addr) {
				found = 1;
				break;
			}
		}

		if (found) {
			page_info->offlined = PAGE_OFFLINE;
			continue;
		}

		/* Time to silence this noisy page */
		if (row_offline_action == OFFLINE_SOFT_THEN_HARD) {
			ret = do_page_offline(page_info->addr, OFFLINE_SOFT);
			if (ret < 0)
				ret = do_page_offline(page_info->addr, OFFLINE_HARD);
		} else {
			ret = do_page_offline(page_info->addr, row_offline_action);
		}

		page_info->offlined  = ret < 0 ? PAGE_OFFLINE_FAILED : PAGE_OFFLINE;

		log(TERM, LOG_INFO,
		    "Result of offlining page at %#llx of row %s: %s\n",
		    page_info->addr, row_id, page_state[page_info->offlined]);

		if (page_info->offlined == PAGE_OFFLINE &&
		    addr_list_size < SAME_PAGE_IN_ROW)
			addr_list[addr_list_size++] = page_info->addr;
	}
}

static void row_record(struct row_record *rr, time_t time)
{
	if (!rr)
		return;

	if (time - rr->start > row_cycle.val) {
		struct page_addr *page_info = NULL, *tmp_page_info = NULL;

		page_info = LIST_FIRST(&rr->page_head);
		while (page_info) {
			// delete exceeds row_cycle.val
			if (time - page_info->start <= row_cycle.val)
				break;
			tmp_page_info = LIST_NEXT(page_info, entry);
			rr->count -= page_info->count;
			LIST_REMOVE(page_info, entry);
			free(page_info);
			page_info = tmp_page_info;
		}
		rr->start = page_info ? page_info->start : time;
	}

	char row_id[ROW_ID_MAX_LEN] = {0};

	row_record_get_id(rr, row_id, ROW_ID_MAX_LEN);
	if (rr->count >= row_threshold.val) {
		log(TERM, LOG_INFO,
		    "Corrected Errors of row %s exceeded row CE threshold, count=%lu\n",
		    row_id, rr->count);
		row_offline(rr, time);
	}
}

static struct row_record *row_lookup_insert(struct row_record *r,
					    unsigned int count,
					    unsigned long long addr,
					    time_t time)
{
	struct row_record *rr = NULL, *new_row_record = NULL;
	struct page_addr *new_page_addr = NULL, *tail_page_addr = NULL;
	int found = 0;

	if (!r)
		return NULL;
	// look same row record
	LIST_FOREACH(rr, &row_head, entry) {
		if (row_record_is_same_row(rr, r)) {
			found = 1;
			new_row_record = rr;
			break;
		}
	}

	// new row
	if (!found) {
		new_row_record = calloc(1, sizeof(struct row_record));
		if (!new_row_record) {
			log(TERM, LOG_ERR, "No memory for new row record\n");
			return NULL;
		}
		new_row_record->start = time;
		new_row_record->count = 0;
		new_row_record->type = r->type;

		LIST_INSERT_HEAD(&row_head, new_row_record, entry);
		row_record_copy(new_row_record, r);
	}

	// new page
	new_page_addr = calloc(1, sizeof(struct page_addr));
	if (!new_page_addr) {
		log(TERM, LOG_ERR, "No memory for new page addr\n");
		return NULL;
	}
	new_page_addr->addr = addr & PAGE_MASK;
	new_page_addr->start = time;
	new_page_addr->count = count;

	struct page_addr *record = NULL;
	int not_empty = 0;

	LIST_FOREACH(record, &new_row_record->page_head, entry) {
		tail_page_addr = record;
		not_empty = 1;
	}
	if (not_empty)
		LIST_INSERT_AFTER(tail_page_addr, new_page_addr, entry);
	else
		LIST_INSERT_HEAD(&new_row_record->page_head, new_page_addr, entry);

	new_row_record->count += new_page_addr->count;

	return new_row_record;
}

void ras_record_row_error(const char *detail, unsigned int count, time_t time, unsigned long long addr)
{
	struct row_record *pr = NULL;
	struct row_record r = {0};

	if (row_offline_action == OFFLINE_OFF)
		return;

	if (parse_row_info(detail, &r))
		return;

	pr = row_lookup_insert(&r, count, addr, time);
	if (!pr) {
		log(TERM, LOG_ERR, "insert CE page structure into CE row structure failed\n");
		return;
	}

	row_record(pr, time);
}

void row_record_infos_free(void)
{
	struct row_record *row_record = NULL, *tmp_row_record = NULL;
	struct page_addr *page_addr = NULL, *tmp_page_addr = NULL;

	row_record = LIST_FIRST(&row_head);
	while (row_record) {
		page_addr = LIST_FIRST(&row_record->page_head);
		while (page_addr) {
			tmp_page_addr = LIST_NEXT(page_addr, entry);
			LIST_REMOVE(page_addr, entry);
			free(page_addr);
			page_addr = tmp_page_addr;
		}
		tmp_row_record = LIST_NEXT(row_record, entry);
		LIST_REMOVE(row_record, entry);
		free(row_record);
		row_record = tmp_row_record;
	}
}

/* memory row CE threshold policy ends */
