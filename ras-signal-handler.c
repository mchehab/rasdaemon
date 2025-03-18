// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2025 Ruidong Tian <tianruidong@linux.alibaba.com>
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
#include <signal.h>

#include "ras-signal-handler.h"
#include "ras-report.h"
#include "types.h"

enum {
	TRACE_SIGNAL_DELIVERED,
	TRACE_SIGNAL_IGNORED,
	TRACE_SIGNAL_ALREADY_PENDING,
	TRACE_SIGNAL_OVERFLOW_FAIL,
	TRACE_SIGNAL_LOSE_INFO,
};

static char *signal_msg[] = {
	[BUS_ADRALN] = "invalid address alignment",
	[BUS_ADRERR] = "non-existent address",
	[BUS_OBJERR] = "object-specific hardware error",
	[BUS_MCEERR_AR] = "Hardware memory error consumed: action required",
	[BUS_MCEERR_AO] = "Hardware memory error detected in process but not consumed: action optional",
};

static char *errcode_str[] = {
	[BUS_ADRALN] = "BUS_ADRALN",
	[BUS_ADRERR] = "BUS_ADRERR",
	[BUS_OBJERR] = "BUS_OBJERR",
	[BUS_MCEERR_AR] = "BUS_MCEERR_AR",
	[BUS_MCEERR_AO] = "BUS_MCEERR_AO",
};

static char *signal_res[] = {
	[TRACE_SIGNAL_DELIVERED] = "Delivered",
	[TRACE_SIGNAL_IGNORED] = "Ignore",
	[TRACE_SIGNAL_ALREADY_PENDING] = "Already pending",
	[TRACE_SIGNAL_OVERFLOW_FAIL] = "Overflow fail",
	[TRACE_SIGNAL_LOSE_INFO] = "Lose info",
};

static void report_ras_signal_event(struct trace_seq *s, struct ras_signal_event *ev)
{
	trace_seq_printf(s,
			 "%s signal: %s, errorno: %d, code: %s, comm: %s, pid: %d, grp: %d, res: %s, msg: %s",
			 ev->timestamp, strsignal(ev->sig), ev->error_no,
			 (ev->code < 0 || ev->code > BUS_MCEERR_AO) ? "Unknown" : errcode_str[ev->code],
			 ev->comm, ev->pid,
			 ev->group,
			 (ev->result < 0 || ev->result > TRACE_SIGNAL_LOSE_INFO) ? "Unknown" : signal_res[ev->result],
			 ev->sig == SIGBUS ? signal_msg[ev->code] : "Unknown");
}

int ras_signal_event_handler(struct trace_seq *s, struct tep_record *record,
			     struct tep_event *event, void *context)
{
	int len;
	unsigned long long val;
	struct ras_events *ras = context;
	time_t now;
	struct tm *tm;
	struct ras_signal_event ev;

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

	if (tep_get_field_val(s,  event, "sig", record, &val, 1) < 0)
		return -1;
	ev.sig = val;

	if (tep_get_field_val(s, event, "errno", record, &val, 1) < 0)
		return -1;
	ev.error_no = val;

	if (tep_get_field_val(s, event, "code", record, &val, 1) < 0)
		return -1;
	ev.code = val;

	ev.comm = tep_get_field_raw(s, event, "comm", record, &len, 1);
	if (!ev.comm)
		return -1;

	if (tep_get_field_val(s, event, "pid", record, &val, 1) < 0)
		return -1;
	ev.pid = val;

	if (tep_get_field_val(s, event, "group", record, &val, 1) < 0)
		return -1;
	ev.group = val;

	if (tep_get_field_val(s, event, "result", record, &val, 1) < 0)
		return -1;
	ev.result = val;

	report_ras_signal_event(s, &ev);

	/* Store data into the SQLite DB */
#ifdef HAVE_SQLITE3
	ras_store_signal_event(ras, &ev);
#endif

#ifdef HAVE_ABRT_REPORT
	/* Report event to ABRT */
	ras_report_signal_event(ras, &ev);
#endif

	return 0;
}
