/*
 * Copyright (C) 2019 Cong Wang <xiyou.wangcong@gmail.com>
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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
*/
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "libtrace/kbuffer.h"
#include "ras-devlink-handler.h"
#include "ras-record.h"
#include "ras-logger.h"
#include "ras-report.h"

int ras_net_xmit_timeout_handler(struct trace_seq *s,
				 struct pevent_record *record,
				 struct event_format *event, void *context)
{
	unsigned long long val;
	int len;
	struct ras_events *ras = context;
	time_t now;
	struct tm *tm;
	struct devlink_event ev;

	if (ras->use_uptime)
		now = record->ts/user_hz + ras->uptime_diff;
	else
		now = time(NULL);

	tm = localtime(&now);
	if (tm)
		strftime(ev.timestamp, sizeof(ev.timestamp),
			 "%Y-%m-%d %H:%M:%S %z", tm);
	trace_seq_printf(s, "%s ", ev.timestamp);

	ev.bus_name = "";
	ev.reporter_name = "";

	ev.dev_name = pevent_get_field_raw(s, event, "name",
					   record, &len, 1);
	if (!ev.dev_name)
		return -1;

	ev.driver_name = pevent_get_field_raw(s, event, "driver",
					   record, &len, 1);
	if (!ev.driver_name)
		return -1;

	if (pevent_get_field_val(s, event, "queue_index", record, &val, 1) < 0)
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
			      struct pevent_record *record,
			      struct event_format *event, void *context)
{
	int len;
	struct ras_events *ras = context;
	time_t now;
	struct tm *tm;
	struct devlink_event ev;

	if (ras->filters[DEVLINK_EVENT] &&
	    pevent_filter_match(ras->filters[DEVLINK_EVENT], record) == FILTER_MATCH)
		return 0;
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
	trace_seq_printf(s, "%s ", ev.timestamp);

	ev.bus_name = pevent_get_field_raw(s, event, "bus_name",
					   record, &len, 1);
	if (!ev.bus_name)
		return -1;

	ev.dev_name = pevent_get_field_raw(s, event, "dev_name",
					   record, &len, 1);
	if (!ev.dev_name)
		return -1;

	ev.driver_name = pevent_get_field_raw(s, event, "driver_name",
					   record, &len, 1);
	if (!ev.driver_name)
		return -1;

	ev.reporter_name = pevent_get_field_raw(s, event, "reporter_name",
					   record, &len, 1);
	if (!ev.reporter_name)
		return -1;

	ev.msg = pevent_get_field_raw(s, event, "msg", record, &len, 1);
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
