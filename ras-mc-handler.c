/*
 * Copyright (C) 2013 Mauro Carvalho Chehab <mchehab+redhat@kernel.org>
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "libtrace/kbuffer.h"
#include "ras-mc-handler.h"
#include "ras-record.h"
#include "ras-logger.h"
#include "ras-report.h"

int ras_mc_event_handler(struct trace_seq *s,
			 struct pevent_record *record,
			 struct event_format *event, void *context)
{
	int len;
	unsigned long long val;
	struct ras_events *ras = context;
	time_t now;
	struct tm *tm;
	struct ras_mc_event ev;
	int parsed_fields = 0;

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

	if (pevent_get_field_val(s,  event, "error_count", record, &val, 1) < 0)
		goto parse_error;
	parsed_fields++;

	ev.error_count = val;
	trace_seq_printf(s, "%d ", ev.error_count);

	if (pevent_get_field_val(s, event, "error_type", record, &val, 1) < 0)
		goto parse_error;
	parsed_fields++;

	switch (val) {
	case HW_EVENT_ERR_CORRECTED:
		ev.error_type = "Corrected";
		break;
	case HW_EVENT_ERR_UNCORRECTED:
		ev.error_type = "Uncorrected";
		break;
	case HW_EVENT_ERR_FATAL:
		ev.error_type = "Fatal";
		break;
	default:
	case HW_EVENT_ERR_INFO:
		ev.error_type = "Info";
	}

	trace_seq_puts(s, ev.error_type);
	if (ev.error_count > 1)
		trace_seq_puts(s, " errors:");
	else
		trace_seq_puts(s, " error:");

	ev.msg = pevent_get_field_raw(s, event, "msg", record, &len, 1);
	if (!ev.msg)
		goto parse_error;
	parsed_fields++;

	if (*ev.msg) {
		trace_seq_puts(s, " ");
		trace_seq_puts(s, ev.msg);
	}

	ev.label = pevent_get_field_raw(s, event, "label", record, &len, 1);
	if (!ev.label)
		goto parse_error;
	parsed_fields++;

	if (*ev.label) {
		trace_seq_puts(s, " on ");
		trace_seq_puts(s, ev.label);
	}

	trace_seq_puts(s, " (");
	if (pevent_get_field_val(s,  event, "mc_index", record, &val, 1) < 0)
		goto parse_error;
	parsed_fields++;

	ev.mc_index = val;
	trace_seq_printf(s, "mc: %d", ev.mc_index);

	if (pevent_get_field_val(s,  event, "top_layer", record, &val, 1) < 0)
		goto parse_error;
	parsed_fields++;
	ev.top_layer = (signed char) val;

	if (pevent_get_field_val(s,  event, "middle_layer", record, &val, 1) < 0)
		goto parse_error;
	parsed_fields++;
	ev.middle_layer = (signed char) val;

	if (pevent_get_field_val(s,  event, "lower_layer", record, &val, 1) < 0)
		goto parse_error;
	parsed_fields++;
	ev.lower_layer = (signed char) val;

	if (ev.top_layer >= 0 || ev.middle_layer >= 0 || ev.lower_layer >= 0) {
		if (ev.lower_layer >= 0)
			trace_seq_printf(s, " location: %d:%d:%d",
					ev.top_layer, ev.middle_layer, ev.lower_layer);
		else if (ev.middle_layer >= 0)
			trace_seq_printf(s, " location: %d:%d",
					 ev.top_layer, ev.middle_layer);
		else
			trace_seq_printf(s, " location: %d", ev.top_layer);
	}

	if (pevent_get_field_val(s,  event, "address", record, &val, 1) < 0)
		goto parse_error;
	parsed_fields++;

	ev.address = val;
	if (ev.address)
		trace_seq_printf(s, " address: 0x%08llx", ev.address);

	if (pevent_get_field_val(s,  event, "grain_bits", record, &val, 1) < 0)
		goto parse_error;
	parsed_fields++;

	ev.grain = val;
	trace_seq_printf(s, " grain: %lld", ev.grain);


	if (pevent_get_field_val(s,  event, "syndrome", record, &val, 1) < 0)
		goto parse_error;
	parsed_fields++;

	ev.syndrome = val;
	if (val)
		trace_seq_printf(s, " syndrome: 0x%08llx", ev.syndrome);

	ev.driver_detail = pevent_get_field_raw(s, event, "driver_detail", record,
					     &len, 1);
	if (!ev.driver_detail)
		goto parse_error;
	parsed_fields++;

	if (*ev.driver_detail) {
		trace_seq_puts(s, " ");
		trace_seq_puts(s, ev.driver_detail);
	}
	trace_seq_puts(s, ")");

	/* Insert data into the SGBD */

	ras_store_mc_event(ras, &ev);

#ifdef HAVE_ABRT_REPORT
	/* Report event to ABRT */
	ras_report_mc_event(ras, &ev);
#endif

	return 0;

parse_error:
	/* FIXME: add a logic here to also store parse errors to SDBD */

	log(ALL, LOG_ERR, "MC error handler: can't parse field #%d\n",
	    parsed_fields);

	return 0;
}
