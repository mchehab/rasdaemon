/*
 * Copyright (C) 2013 Mauro Carvalho Chehab <mchehab@redhat.com>
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
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/poll.h>
#include "libtrace/kbuffer.h"
#include "ras-mc-handler.h"
#include "ras-record.h"
#include "ras-logger.h"

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
	char fmt[64];

	/*
	 * Newer kernels (3.10-rc1 or upper) provide an uptime clock.
	 * On previous kernels, the way to properly generate an event would
	 * be to inject a fake one, measure its timestamp and diff it against
	 * gettimeofday. We won't do it here. Instead, let's use uptime,
	 * falling-back to the event report's time, if "uptime" clock is
	 * not available (legacy kernels).
	 */

	if (ras->use_uptime)
		now = record->ts/1000000000L + ras->uptime_diff;
	else
		now = time(NULL);

	tm = localtime(&now);
	if (tm)
		strftime(ev.timestamp, sizeof(ev.timestamp),
			 "%Y-%m-%d %H:%M:%S %z", tm);
	trace_seq_printf(s, "%s ", ev.timestamp);

	if (pevent_get_field_val(s,  event, "error_count", record, &val, 1) < 0)
		return -1;
	ev.error_count = val;
	trace_seq_printf(s, "%d ", ev.error_count);

	if (pevent_get_field_val(s, event, "error_type", record, &val, 1) < 0)
		return -1;
	ev.error_type = mc_event_error_type(val);
	trace_seq_puts(s, ev.error_type);
	if (ev.error_count > 1)
		trace_seq_puts(s, " errors:");
	else
		trace_seq_puts(s, " error:");

	ev.msg = pevent_get_field_raw(s, event, "msg", record, &len, 1);
	if (!ev.msg)
		return -1;
	if (*ev.msg) {
		trace_seq_puts(s, " ");
		trace_seq_puts(s, ev.msg);
	}

	ev.label = pevent_get_field_raw(s, event, "label", record, &len, 1);
	if (!ev.label)
		return -1;
	if (*ev.label) {
		trace_seq_puts(s, " on ");
		trace_seq_puts(s, ev.label);
	}

	trace_seq_puts(s, " (");
	if (pevent_get_field_val(s,  event, "mc_index", record, &val, 1) < 0)
		return -1;
	ev.mc_index = val;
	trace_seq_printf(s, "mc: %d", ev.mc_index);

	if (pevent_get_field_val(s,  event, "top_layer", record, &val, 1) < 0)
		return -1;
	ev.top_layer = (int) val;
	if (pevent_get_field_val(s,  event, "middle_layer", record, &val, 1) < 0)
		return -1;
	ev.middle_layer = (int) val;
	if (pevent_get_field_val(s,  event, "lower_layer", record, &val, 1) < 0)
		return -1;
	ev.lower_layer = (int) val;

	if (ev.top_layer == 255)
		ev.top_layer = -1;
	if (ev.middle_layer == 255)
		ev.middle_layer = -1;
	if (ev.lower_layer == 255)
		ev.lower_layer = -1;

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
		return -1;
	ev.address = val;
	if (ev.address)
		trace_seq_printf(s, " address: 0x%08llx", ev.address);

	if (pevent_get_field_val(s,  event, "grain_bits", record, &val, 1) < 0)
		return -1;
	ev.grain = val;
	trace_seq_printf(s, " grain: %lld", ev.grain);


	if (pevent_get_field_val(s,  event, "syndrome", record, &val, 1) < 0)
		return -1;
	ev.syndrome = val;
	if (val)
		trace_seq_printf(s, " syndrome: 0x%08llx", ev.syndrome);

	ev.driver_detail = pevent_get_field_raw(s, event, "driver_detail", record,
					     &len, 1);
	if (!ev.driver_detail)
		return -1;
	if (*ev.driver_detail) {
		trace_seq_puts(s, " ");
		trace_seq_puts(s, ev.driver_detail);
	}
	trace_seq_puts(s, ")");

	/* Insert data into the SGBD */

	ras_store_mc_event(ras, &ev);

	return 0;
}
