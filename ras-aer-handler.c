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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "libtrace/kbuffer.h"
#include "ras-aer-handler.h"
#include "ras-record.h"
#include "ras-logger.h"
#include "bitfield.h"
#include "ras-report.h"

static const char *aer_errors[32] = {
	/* Correctable errors */
	[0]  = "Receiver Error",
	[6]  = "Bad TLP",
	[7]  = "Bad DLLP",
	[8]  = "RELAY_NUM Rollover",
	[12] = "Replay Timer Timeout",
	[13] = "Advisory Non-Fatal",

	/* Uncorrectable errors */
	[4]  = "Data Link Protocol",
	[12] = "Poisoned TLP",
	[13] = "Flow Control Protocol",
	[14] = "Completion Timeout",
	[15] = "Completer Abort",
	[16] = "Unexpected Completion",
	[17] = "Receiver Overflow",
	[18] = "Malformed TLP",
	[19] = "ECRC",
	[20] = "Unsupported Request",
};

int ras_aer_event_handler(struct trace_seq *s,
			 struct pevent_record *record,
			 struct event_format *event, void *context)
{
	int len;
	unsigned long long val;
	struct ras_events *ras = context;
	time_t now;
	struct tm *tm;
	struct ras_aer_event ev;
	char buf[1024];

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

	ev.dev_name = pevent_get_field_raw(s, event, "dev_name",
					   record, &len, 1);
	if (!ev.dev_name)
		return -1;

	if (pevent_get_field_val(s,  event, "status", record, &val, 1) < 0)
		return -1;

	/* Fills the error buffer */
	bitfield_msg(buf, sizeof(buf), aer_errors, 32, 0, 0, val);
	ev.msg = buf;
	trace_seq_printf(s, "%s ", ev.msg);

	if (pevent_get_field_val(s, event, "severity", record, &val, 1) < 0)
		return -1;
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

	/* Insert data into the SGBD */
#ifdef HAVE_SQLITE3
	ras_store_aer_event(ras, &ev);
#endif

#ifdef HAVE_ABRT_REPORT
	/* Report event to ABRT */
	ras_report_aer_event(ras, &ev);
#endif

	return 0;
}
