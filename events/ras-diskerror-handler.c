// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * Copyright (C) 2019 Cong Wang <xiyou.wangcong@gmail.com>
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <traceevent/kbuffer.h>

#include "core/modules.h"
#include "core/ras-logger.h"
#include "core/types.h"
#include "events/ras-diskerror-handler.h"

int ras_diskerror_event_handler(struct trace_seq *s,
				struct tep_record *record,
				struct tep_event *event, void *context);
int db_diskerror_event(struct ras_events *ras, void *priv);

#ifdef HAVE_UNITTEST
int test_diskerror(void) __attribute__((weak));
#endif

#ifndef HAVE_BLK_RQ_ERROR
static int ras_diskerror_prepare(struct ras_events *ras)
{
	return ras_event_filter(ras, "block", "block_rq_complete",
				"error != 0");
}
#endif

static const struct ras_event_entry ras_diskerror_event = {
	.group = "block",
#ifdef HAVE_BLK_RQ_ERROR
	.event = "block_rq_error",
#else
	.event = "block_rq_complete", .prepare = ras_diskerror_prepare,
#endif
	.handler = ras_diskerror_event_handler, .id = DISKERROR_EVENT,
	.trigger = true,
#ifdef HAVE_UNITTEST
	.test_group = TEST_GROUP_EVENTS, .test = test_diskerror,
#endif
	.record = db_diskerror_event,
};
REGISTER_RAS_EVENT(ras_diskerror_event);
#include "modules/ras-report.h"

static const struct {
	int             error;
	const char      *name;
} blk_errors[] = {
	{ -EOPNOTSUPP, "operation not supported error" },
	{ -ETIMEDOUT, "timeout error" },
	{ -ENOSPC,    "critical space allocation error" },
	{ -ENOLINK,   "recoverable transport error" },
	{ -EREMOTEIO, "critical target error" },
	{ -EBADE,     "critical nexus error" },
	{ -ENODATA,   "critical medium error" },
	{ -EILSEQ,    "protection error" },
	{ -ENOMEM,    "kernel resource error" },
	{ -EBUSY,     "device resource error" },
	{ -EAGAIN,    "nonblocking retry error" },
	{ -EREMCHG, "dm internal retry error" },
	{ -EIO,       "I/O error" },
};

static const char *get_blk_error(int err)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(blk_errors); i++)
		if (blk_errors[i].error == err)
			return blk_errors[i].name;
	return "unknown block error";
}

#ifdef HAVE_UNITTEST
const char *ras_diskerror_test_error(int err)
{
	return get_blk_error(err);
}
#endif

int ras_diskerror_event_handler(struct trace_seq *s,
				struct tep_record *record,
				struct tep_event *event, void *context)
{
	unsigned long long val;
	int len;
	struct ras_events *ras = context;
	time_t now;
	struct tm *tm;
	struct diskerror_event ev;
	uint32_t dev;

	trace_seq_printf(s, "%s ", loglevel_str[LOGLEVEL_ERR]);
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
	trace_seq_printf(s, "%s ", ev.timestamp);

	if (tep_get_field_val(s, event, "dev", record, &val, 1) < 0)
		return -1;
	dev = (uint32_t)val;
	if (asprintf(&ev.dev, "%u:%u", MAJOR(dev), MINOR(dev)) < 0)
		return -1;

	if (tep_get_field_val(s, event, "sector", record, &val, 1) < 0)
		return -1;
	ev.sector = val;

	if (tep_get_field_val(s, event, "nr_sector", record, &val, 1) < 0)
		return -1;
	ev.nr_sector = (unsigned int)val;

	if (tep_get_field_val(s, event, "error", record, &val, 1) < 0)
		return -1;
	ev.error = get_blk_error((int)val);

	ev.rwbs = tep_get_field_raw(s, event, "rwbs", record, &len, 1);
	if (!ev.rwbs)
		return -1;

	ev.cmd = tep_get_field_raw(s, event, "cmd", record, &len, 1);
	if (!ev.cmd)
		return -1;

	/* Insert data into the SGBD */
	db_diskerror_event(ras, &ev);


#ifdef HAVE_ABRT_REPORT
	/* Report event to ABRT */
	ras_report_diskerror_event(ras, &ev);
#endif
	free(ev.dev);
	return 0;
}
static const struct db_fields diskerror_event_fields[] = {
	{ .name = "id",			.type = DB_TYPE_SERIAL, .is_pk = true },
	{ .name = "timestamp",		.type = DB_TYPE_TIMESTAMP, .create_index = true },
	{ .name = "dev",		.type = DB_TYPE_TEXT },
	{ .name = "sector",		.type = DB_TYPE_INT64 },
	{ .name = "nr_sector",		.type = DB_TYPE_INT32 },
	{ .name = "error",		.type = DB_TYPE_TEXT },
	{ .name = "rwbs",		.type = DB_TYPE_TEXT },
	{ .name = "cmd",		.type = DB_TYPE_TEXT },
};

const struct db_table_descriptor diskerror_event_tab = {
	.name = "disk_errors",
	.fields = diskerror_event_fields,
	.num_fields = ARRAY_SIZE(diskerror_event_fields),
};

static struct db_desc_and_stmt diskerror_event_db = {
	.desc = &diskerror_event_tab,
};

int db_diskerror_event(struct ras_events *ras, void *priv)
{
	struct diskerror_event *ev = priv;
	int rc, pos = 1;

	if (!diskerror_event_db.stmt)
		return 0;
	log(TERM, LOG_INFO, "diskerror_event store: %p\n", diskerror_event_db.stmt);

	db_bind(&diskerror_event_tab, diskerror_event_db.stmt, pos++, (uint64_t)ev->timestamp, -1);
	db_bind(&diskerror_event_tab, diskerror_event_db.stmt, pos++, (uint64_t)ev->dev, -1);
	db_bind(&diskerror_event_tab, diskerror_event_db.stmt, pos++, ev->sector, -1);
	db_bind(&diskerror_event_tab, diskerror_event_db.stmt, pos++, ev->nr_sector, -1);
	db_bind(&diskerror_event_tab, diskerror_event_db.stmt, pos++, (uint64_t)ev->error, -1);
	db_bind(&diskerror_event_tab, diskerror_event_db.stmt, pos++, (uint64_t)ev->rwbs, -1);
	db_bind(&diskerror_event_tab, diskerror_event_db.stmt, pos++, (uint64_t)ev->cmd, -1);

	rc = db_eval_stmt(diskerror_event_db.stmt, "diskerror_event");
	if (!rc)
		log(TERM, LOG_INFO, "register inserted at db\n");

	return rc;
}

static int ras_diskerror_db_init(struct ras_module_ctx *ctx)
{
	return ras_db_table_register(ctx, &diskerror_event_db);
}

static void ras_diskerror_db_cleanup(struct ras_module_ctx *ctx)
{
	ras_db_table_unregister(ctx);
}

static const struct ras_module_entry ras_diskerror_module = {
	.name = "disk-error-event",
	.level = BASE_EVENT_MODULE,
	.init = ras_diskerror_db_init,
	.cleanup = ras_diskerror_db_cleanup,
};

static void __attribute__((constructor)) ras_diskerror_register(void)
{
	int rc = module_register(&ras_diskerror_module);

	if (rc)
		log(TERM, LOG_ERR, "Failed to register disk-error module: %d\n", rc);
}
