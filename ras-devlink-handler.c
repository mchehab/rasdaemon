// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * Copyright (C) 2019 Cong Wang <xiyou.wangcong@gmail.com>
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <traceevent/kbuffer.h>
#include <unistd.h>

#include "ras-devlink-handler.h"
#include "ras-logger.h"
#include "ras-report.h"
#include "types.h"

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
#ifdef HAVE_SQLITE3
	ras_store_devlink_event(ras, &ev);
#endif

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
#ifdef HAVE_SQLITE3
	ras_store_devlink_event(ras, &ev);
#endif

#ifdef HAVE_ABRT_REPORT
	/* Report event to ABRT */
	ras_report_devlink_event(ras, &ev);
#endif

	return 0;
}
