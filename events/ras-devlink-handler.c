// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * Copyright (C) 2019 Cong Wang <xiyou.wangcong@gmail.com>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <traceevent/kbuffer.h>
#include <unistd.h>

#include "core/modules.h"
#include "core/ras-logger.h"
#include "core/types.h"
#include "events/ras-devlink-handler.h"

int ras_net_xmit_timeout_handler(struct trace_seq *s,
				 struct tep_record *record,
				 struct tep_event *event, void *context);
int ras_devlink_event_handler(struct trace_seq *s, struct tep_record *record,
			      struct tep_event *event, void *context);
int db_devlink_event(struct ras_events *ras, void *priv);

static bool net_timeout_enabled;

static int ras_net_timeout_prepare(struct ras_events *ras)
{
	net_timeout_enabled = false;
	return 0;
}

#ifdef HAVE_UNITTEST
int test_devlink(void) __attribute__((weak));
#endif

static void ras_net_timeout_enabled(struct ras_events *ras)
{
	net_timeout_enabled = true;
}

static const char *ras_devlink_filter(struct ras_events *ras)
{
	return net_timeout_enabled ?
		"devlink/devlink_health_report:msg=~'TX timeout*'" : NULL;
}

static const struct ras_event_entry ras_net_timeout_event = {
	.group = "net", .event = "net_dev_xmit_timeout",
	.handler = ras_net_xmit_timeout_handler, .id = DEVLINK_EVENT,
	.trigger = true, .prepare = ras_net_timeout_prepare,
	.enabled = ras_net_timeout_enabled,
};
REGISTER_RAS_EVENT(ras_net_timeout_event);

static const struct ras_event_entry ras_devlink_event = {
	.group = "devlink", .event = "devlink_health_report",
	.handler = ras_devlink_event_handler, .filter_cb = ras_devlink_filter,
	.id = DEVLINK_EVENT, .order = 1, .trigger = true,
#ifdef HAVE_UNITTEST
	.test_group = TEST_GROUP_EVENTS, .test = test_devlink,
#endif
	.record = db_devlink_event,
};
REGISTER_RAS_EVENT(ras_devlink_event);
#include "modules/ras-report.h"

int ras_net_xmit_timeout_handler(struct trace_seq *s,
				 struct tep_record *record,
				 struct tep_event *event, void *context)
{
	unsigned long long val;
	int len;
	struct ras_events *ras = context;
	time_t now;
	struct tm *tm;
	struct devlink_event ev;

	if (ras->use_uptime)
		now = record->ts / user_hz + ras->uptime_diff;
	else
		now = time(NULL);

	tm = localtime(&now);
	if (tm)
		strftime(ev.timestamp, sizeof(ev.timestamp),
			 "%Y-%m-%d %H:%M:%S %z", tm);
	trace_seq_printf(s, "%s ", ev.timestamp);

	ev.bus_name = "";
	ev.reporter_name = "";

	ev.dev_name = tep_get_field_raw(s, event, "name",
					record, &len, 1);
	if (!ev.dev_name)
		return -1;

	ev.driver_name = tep_get_field_raw(s, event, "driver",
					   record, &len, 1);
	if (!ev.driver_name)
		return -1;

	if (tep_get_field_val(s, event, "queue_index", record, &val, 1) < 0)
		return -1;
	if (asprintf(&ev.msg, "TX timeout on queue: %d\n", (int)val) < 0)
		return -1;

	/* Insert data into the SGBD */
	db_devlink_event(ras, &ev);


#ifdef HAVE_ABRT_REPORT
	/* Report event to ABRT */
	ras_report_devlink_event(ras, &ev);
#endif

	free(ev.msg);
	return 0;
}

int ras_devlink_event_handler(struct trace_seq *s,
			      struct tep_record *record,
			      struct tep_event *event, void *context)
{
	int len;
	struct ras_events *ras = context;
	time_t now;
	struct tm *tm;
	struct devlink_event ev;

	if (ras->filters[DEVLINK_EVENT] &&
	    tep_filter_match(ras->filters[DEVLINK_EVENT], record) == FILTER_MATCH)
		return 0;

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

	ev.bus_name = tep_get_field_raw(s, event, "bus_name",
					record, &len, 1);
	if (!ev.bus_name)
		return -1;

	ev.dev_name = tep_get_field_raw(s, event, "dev_name",
					record, &len, 1);
	if (!ev.dev_name)
		return -1;

	ev.driver_name = tep_get_field_raw(s, event, "driver_name",
					   record, &len, 1);
	if (!ev.driver_name)
		return -1;

	ev.reporter_name = tep_get_field_raw(s, event, "reporter_name",
					     record, &len, 1);
	if (!ev.reporter_name)
		return -1;

	ev.msg = tep_get_field_raw(s, event, "msg", record, &len, 1);
	if (!ev.msg)
		return -1;

	/* Insert data into the SGBD */
	db_devlink_event(ras, &ev);


#ifdef HAVE_ABRT_REPORT
	/* Report event to ABRT */
	ras_report_devlink_event(ras, &ev);
#endif

	return 0;
}
static const struct db_fields devlink_event_fields[] = {
	{ .name = "id",			.type = DB_TYPE_SERIAL, .is_pk = true },
	{ .name = "timestamp",		.type = DB_TYPE_TIMESTAMP, .create_index = true },
	{ .name = "bus_name",		.type = DB_TYPE_TEXT },
	{ .name = "dev_name",		.type = DB_TYPE_TEXT },
	{ .name = "driver_name",	.type = DB_TYPE_TEXT },
	{ .name = "reporter_name",	.type = DB_TYPE_TEXT },
	{ .name = "msg",		.type = DB_TYPE_TEXT },
};

const struct db_table_descriptor devlink_event_tab = {
	.name = "devlink_event",
	.fields = devlink_event_fields,
	.num_fields = ARRAY_SIZE(devlink_event_fields),
};

static struct db_desc_and_stmt devlink_event_db = {
	.desc = &devlink_event_tab,
};

int db_devlink_event(struct ras_events *ras, void *priv)
{
	struct devlink_event *ev = priv;
	int rc, pos = 1;

	if (!devlink_event_db.stmt)
		return 0;
	log(TERM, LOG_INFO, "devlink_event store: %p\n", devlink_event_db.stmt);

	db_bind(&devlink_event_tab, devlink_event_db.stmt, pos++, (uint64_t)ev->timestamp, -1);
	db_bind(&devlink_event_tab, devlink_event_db.stmt, pos++, (uint64_t)ev->bus_name, -1);
	db_bind(&devlink_event_tab, devlink_event_db.stmt, pos++, (uint64_t)ev->dev_name, -1);
	db_bind(&devlink_event_tab, devlink_event_db.stmt, pos++, (uint64_t)ev->driver_name, -1);
	db_bind(&devlink_event_tab, devlink_event_db.stmt, pos++, (uint64_t)ev->reporter_name, -1);
	db_bind(&devlink_event_tab, devlink_event_db.stmt, pos++, (uint64_t)ev->msg, -1);

	rc = db_eval_stmt(devlink_event_db.stmt, "devlink_event");
	if (!rc)
		log(TERM, LOG_INFO, "register inserted at db\n");

	return rc;
}

static int ras_devlink_db_init(struct ras_module_ctx *ctx)
{
	return ras_db_table_register(ctx, &devlink_event_db);
}

static void ras_devlink_db_cleanup(struct ras_module_ctx *ctx)
{
	ras_db_table_unregister(ctx);
}

static const struct ras_module_entry ras_devlink_module = {
	.name = "devlink-event",
	.level = BASE_EVENT_MODULE,
	.init = ras_devlink_db_init,
	.cleanup = ras_devlink_db_cleanup,
};

static void __attribute__((constructor)) ras_devlink_register(void)
{
	int rc = module_register(&ras_devlink_module);

	if (rc)
		log(TERM, LOG_ERR, "Failed to register devlink module: %d\n", rc);
}
