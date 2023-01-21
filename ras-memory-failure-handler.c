/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2020. All rights reserved.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <traceevent/kbuffer.h>
#include "ras-memory-failure-handler.h"
#include "ras-record.h"
#include "ras-logger.h"
#include "ras-report.h"

/* Memory failure - various types of pages */
enum mf_action_page_type {
	MF_MSG_KERNEL,
	MF_MSG_KERNEL_HIGH_ORDER,
	MF_MSG_SLAB,
	MF_MSG_DIFFERENT_COMPOUND,
	MF_MSG_POISONED_HUGE,
	MF_MSG_HUGE,
	MF_MSG_FREE_HUGE,
	MF_MSG_NON_PMD_HUGE,
	MF_MSG_UNMAP_FAILED,
	MF_MSG_DIRTY_SWAPCACHE,
	MF_MSG_CLEAN_SWAPCACHE,
	MF_MSG_DIRTY_MLOCKED_LRU,
	MF_MSG_CLEAN_MLOCKED_LRU,
	MF_MSG_DIRTY_UNEVICTABLE_LRU,
	MF_MSG_CLEAN_UNEVICTABLE_LRU,
	MF_MSG_DIRTY_LRU,
	MF_MSG_CLEAN_LRU,
	MF_MSG_TRUNCATED_LRU,
	MF_MSG_BUDDY,
	MF_MSG_BUDDY_2ND,
	MF_MSG_DAX,
	MF_MSG_UNSPLIT_THP,
	MF_MSG_UNKNOWN,
};

/* Action results for various types of pages */
enum mf_action_result {
	MF_IGNORED,     /* Error: cannot be handled */
	MF_FAILED,      /* Error: handling failed */
	MF_DELAYED,     /* Will be handled later */
	MF_RECOVERED,   /* Successfully recovered */
};

/* memory failure page types */
static const struct {
	int	type;
	const char	*page_type;
} mf_page_type[] = {
	{ MF_MSG_KERNEL, "reserved kernel page" },
	{ MF_MSG_KERNEL_HIGH_ORDER, "high-order kernel page"},
	{ MF_MSG_SLAB, "kernel slab page"},
	{ MF_MSG_DIFFERENT_COMPOUND, "different compound page after locking"},
	{ MF_MSG_POISONED_HUGE, "huge page already hardware poisoned"},
	{ MF_MSG_HUGE, "huge page"},
	{ MF_MSG_FREE_HUGE, "free huge page"},
	{ MF_MSG_NON_PMD_HUGE, "non-pmd-sized huge page"},
	{ MF_MSG_UNMAP_FAILED, "unmapping failed page"},
	{ MF_MSG_DIRTY_SWAPCACHE, "dirty swapcache page"},
	{ MF_MSG_CLEAN_SWAPCACHE, "clean swapcache page"},
	{ MF_MSG_DIRTY_MLOCKED_LRU, "dirty mlocked LRU page"},
	{ MF_MSG_CLEAN_MLOCKED_LRU, "clean mlocked LRU page"},
	{ MF_MSG_DIRTY_UNEVICTABLE_LRU, "dirty unevictable LRU page"},
	{ MF_MSG_CLEAN_UNEVICTABLE_LRU, "clean unevictable LRU page"},
	{ MF_MSG_DIRTY_LRU, "dirty LRU page"},
	{ MF_MSG_CLEAN_LRU, "clean LRU page"},
	{ MF_MSG_TRUNCATED_LRU, "already truncated LRU page"},
	{ MF_MSG_BUDDY, "free buddy page"},
	{ MF_MSG_BUDDY_2ND, "free buddy page (2nd try)"},
	{ MF_MSG_DAX, "dax page"},
	{ MF_MSG_UNSPLIT_THP, "unsplit thp"},
	{ MF_MSG_UNKNOWN, "unknown page"},
};

/* memory failure action results */
static const struct {
	int result;
	const char *action_result;
} mf_action_result[] = {
	{ MF_IGNORED, "Ignored" },
	{ MF_FAILED, "Failed" },
	{ MF_DELAYED, "Delayed" },
	{ MF_RECOVERED, "Recovered" },
};

static const char *get_page_type(int page_type)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(mf_page_type); i++)
		if (mf_page_type[i].type == page_type)
			return mf_page_type[i].page_type;

	return "unknown page";
}

static const char *get_action_result(int result)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(mf_action_result); i++)
		if (mf_action_result[i].result == result)
			return mf_action_result[i].action_result;

	return "unknown";
}


int ras_memory_failure_event_handler(struct trace_seq *s,
				     struct tep_record *record,
				     struct tep_event *event, void *context)
{
	unsigned long long val;
	struct ras_events *ras = context;
	time_t now;
	struct tm *tm;
	struct ras_mf_event ev;

	/*
	 * Newer kernels (3.10-rc1 or upper) provide an uptime clock.
	 * On previous kernels, the way to properly generate an event would
	 * be to inject a fake one, measure its timestamp and diff it against
	 * gettimeofday. We won't do it here. Instead, let's use uptime,
	 * falling-back to the event report's time, if "uptime" clock is
	 * not available (legacy kernels).
	 */

	if (ras->use_uptime)
		now = record->ts/user_hz + ras->uptime_diff;
	else
		now = time(NULL);

	tm = localtime(&now);
	if (tm)
		strftime(ev.timestamp, sizeof(ev.timestamp),
			 "%Y-%m-%d %H:%M:%S %z", tm);
	else
		strncpy(ev.timestamp, "1970-01-01 00:00:00 +0000", sizeof(ev.timestamp));
	trace_seq_printf(s, "%s ", ev.timestamp);

	if (tep_get_field_val(s,  event, "pfn", record, &val, 1) < 0)
		return -1;
	sprintf(ev.pfn, "0x%llx", val);
	trace_seq_printf(s, "pfn=0x%llx ", val);

	if (tep_get_field_val(s, event, "type", record, &val, 1) < 0)
		return -1;
	ev.page_type = get_page_type(val);
	trace_seq_printf(s, "page_type=%s ", ev.page_type);

	if (tep_get_field_val(s, event, "result", record, &val, 1) < 0)
		return -1;
	ev.action_result = get_action_result(val);
	trace_seq_printf(s, "action_result=%s ", ev.action_result);

	/* Store data into the SQLite DB */
#ifdef HAVE_SQLITE3
	ras_store_mf_event(ras, &ev);
#endif

#ifdef HAVE_ABRT_REPORT
	/* Report event to ABRT */
	ras_report_mf_event(ras, &ev);
#endif

	return 0;
}
