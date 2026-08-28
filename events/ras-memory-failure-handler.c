// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2020. All rights reserved.
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/modules.h"
#include "core/ras-logger.h"
#include "core/trigger.h"
#include "core/types.h"
#include "db/ras-db.h"
#include "events/ras-memory-failure-handler.h"

static int ras_memory_failure_event_handler(struct trace_seq *s,
					    struct tep_record *record,
					    struct tep_event *event,
					    void *context);
static int db_mf_event(struct ras_events *ras, void *priv);
static void mem_fail_event_trigger_setup(void);

#ifdef HAVE_UNITTEST
int test_memory_failure(void) __attribute__((weak));
#endif

static const struct ras_event_entry ras_memory_failure_event = {
	.group = "ras", .event = "memory_failure_event",
	.handler = ras_memory_failure_event_handler, .id = MF_EVENT,
	.trigger_setup = mem_fail_event_trigger_setup,
#ifdef HAVE_UNITTEST
	.test_group = TEST_GROUP_EVENTS, .test = test_memory_failure,
#endif
	.record = db_mf_event,
};
REGISTER_RAS_EVENT(ras_memory_failure_event);

/* Memory failure - various types of pages */
enum mf_action_page_type {
	MF_MSG_KERNEL,
	MF_MSG_KERNEL_HIGH_ORDER,
	MF_MSG_SLAB,
	MF_MSG_DIFFERENT_COMPOUND,
	MF_MSG_HUGE,
	MF_MSG_FREE_HUGE,
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
	{ MF_MSG_HUGE, "huge page"},
	{ MF_MSG_FREE_HUGE, "free huge page"},
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

#define MAX_ENV 6
static const char *mf_trigger = NULL;

static void mem_fail_event_trigger_setup(void)
{
	const char *trigger;

	trigger = getenv("MEM_FAIL_TRIGGER");
	if (trigger && strcmp(trigger, "")) {
		mf_trigger = trigger_check(trigger);

		if (!mf_trigger) {
			log(ALL, LOG_ERR,
			    "Cannot access memory_fail_event trigger `%s`\n",
			    trigger);
		} else {
			log(ALL, LOG_INFO,
			    "Setup memory_fail_event trigger `%s`\n",
			    trigger);
		}
	}
}

static void run_mf_trigger(struct ras_mf_event *ev)
{
	char *env[MAX_ENV];
	int ei = 0;
	int i;

	if (!mf_trigger)
		return;

	if (asprintf(&env[ei++], "PATH=%s", getenv("PATH") ?: "/sbin:/usr/sbin:/bin:/usr/bin") < 0)
		goto free;
	if (asprintf(&env[ei++], "TIMESTAMP=%s", ev->timestamp) < 0)
		goto free;
	if (asprintf(&env[ei++], "PFN=%s", ev->pfn) < 0)
		goto free;
	if (asprintf(&env[ei++], "PAGE_TYPE=%s", ev->page_type) < 0)
		goto free;
	if (asprintf(&env[ei++], "ACTION_RESULT=%s", ev->action_result) < 0)
		goto free;

	env[ei] = NULL;
	assert(ei < MAX_ENV);

	run_trigger(mf_trigger, NULL, env, "memory_fail_event");

free:
	for (i = 0; i < ei; i++)
		free(env[i]);
}

static const char *get_page_type(int page_type)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(mf_page_type); i++)
		if (mf_page_type[i].type == page_type)
			return mf_page_type[i].page_type;

	return "unknown page";
}

static const char *get_action_result(int result)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(mf_action_result); i++)
		if (mf_action_result[i].result == result)
			return mf_action_result[i].action_result;

	return "unknown";
}

#ifdef HAVE_UNITTEST
const char *ras_memory_failure_test_page_type(int page_type)
{
	return get_page_type(page_type);
}

const char *ras_memory_failure_test_action_result(int result)
{
	return get_action_result(result);
}
#endif

static int ras_memory_failure_event_handler(struct trace_seq *s,
					    struct tep_record *record,
					    struct tep_event *event,
					    void *context)
{
	unsigned long long val;
	struct ras_events *ras = context;
	time_t now;
	struct tm *tm;
	struct ras_mf_event ev;

	trace_seq_printf(s, "%s ", loglevel_str[LOGLEVEL_ALERT]);
	/*
	 * Newer kernels (3.10-rc1 or upper) provide an uptime clock.
	 * On previous kernels, the way to properly generate an event would
	 * be to inject a fake one, measure its timestamp and diff it against
	 * gettimeofday. We won't do it here. Instead, let's use uptime,
	 * falling-back to the event report's time, if "uptime" clock is
	 * not available (legacy kernels).
	 */

	if (ras->use_uptime)
		now = record->ts / user_hz + ras->uptime_diff;
	else
		now = time(NULL);

	tm = localtime(&now);
	if (tm)
		strftime(ev.timestamp, sizeof(ev.timestamp),
			 "%Y-%m-%d %H:%M:%S %z", tm);
	else
		strscpy(ev.timestamp, "1970-01-01 00:00:00 +0000", sizeof(ev.timestamp));
	trace_seq_printf(s, "%s ", ev.timestamp);

	if (tep_get_field_val(s,  event, "pfn", record, &val, 1) < 0)
		return -1;
	snprintf(ev.pfn, sizeof(ev.pfn), "0x%llx", val);
	trace_seq_printf(s, "pfn=0x%llx ", val);

	if (tep_get_field_val(s, event, "type", record, &val, 1) < 0)
		return -1;
	ev.page_type = get_page_type(val);
	trace_seq_printf(s, "page_type=%s ", ev.page_type);

	if (tep_get_field_val(s, event, "result", record, &val, 1) < 0)
		return -1;
	ev.action_result = get_action_result(val);
	trace_seq_printf(s, "action_result=%s ", ev.action_result);

	ras_event_publish(ras, MF_EVENT, &ev);
	run_mf_trigger(&ev);

	return 0;
}
static const struct db_fields mf_event_fields[] = {
	{ .name = "id",			.type = DB_TYPE_SERIAL, .is_pk = true },
	{ .name = "timestamp",		.type = DB_TYPE_TIMESTAMP, .create_index = true },
	{ .name = "pfn",		.type = DB_TYPE_TEXT },
	{ .name = "page_type",		.type = DB_TYPE_TEXT },
	{ .name = "action_result",	.type = DB_TYPE_TEXT },
};

static const struct db_table_descriptor mf_event_tab = {
	.name = "memory_failure_event",
	.fields = mf_event_fields,
	.num_fields = ARRAY_SIZE(mf_event_fields),
};

static struct db_desc_and_stmt mf_event_db = {
	.desc = &mf_event_tab,
};

static int db_mf_event(struct ras_events *ras, void *priv)
{
	struct ras_mf_event *ev = priv;
	int rc, pos = 1;

	if (!mf_event_db.stmt)
		return 0;
	log(TERM, LOG_INFO, "memory_failure_event store: %p\n", mf_event_db.stmt);

	db_bind(&mf_event_tab, mf_event_db.stmt, pos++, (uint64_t)ev->timestamp, -1);
	db_bind(&mf_event_tab, mf_event_db.stmt, pos++, (uint64_t)ev->pfn, -1);
	db_bind(&mf_event_tab, mf_event_db.stmt, pos++, (uint64_t)ev->page_type, -1);
	db_bind(&mf_event_tab, mf_event_db.stmt, pos++, (uint64_t)ev->action_result, -1);

	rc = db_eval_stmt(mf_event_db.stmt, "mf_event");
	if (!rc)
		log(TERM, LOG_INFO, "register inserted at db\n");

	return rc;
}

static int ras_memory_failure_db_init(struct ras_module_ctx *ctx)
{
	return ras_db_table_register(ctx, &mf_event_db);
}

static void ras_memory_failure_db_cleanup(struct ras_module_ctx *ctx)
{
	ras_db_table_unregister(ctx);
}

static const struct ras_module_entry ras_memory_failure_module = {
	.name = "memory-failure-event",
	.level = BASE_EVENT_MODULE,
	.init = ras_memory_failure_db_init,
	.cleanup = ras_memory_failure_db_cleanup,
};

static void __attribute__((constructor)) ras_memory_failure_register(void)
{
	int rc = module_register(&ras_memory_failure_module);

	if (rc)
		log(TERM, LOG_ERR, "Failed to register memory-failure module: %d\n", rc);
}
