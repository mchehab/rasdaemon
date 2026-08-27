// SPDX-License-Identifier: GPL-2.0-only
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
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/modules.h"
#include "core/ras-logger.h"
#include "core/types.h"
#include "events/ras-signal-handler.h"

#ifdef HAVE_UNITTEST
int test_signal(void) __attribute__((weak));
#endif

static int ras_signal_prepare(struct ras_events *ras)
{
	char filter[64];

	snprintf(filter, sizeof(filter), "sig == %d && code >= %d",
		 SIGBUS, BUS_OBJERR);
	usleep(30000);
	return ras_event_filter(ras, "signal", "signal_generate", filter);
}

static const struct ras_event_entry ras_signal_event = {
	.group = "signal", .event = "signal_generate",
	.handler = ras_signal_event_handler, .id = SIGNAL_EVENT,
	.trigger = true, .prepare = ras_signal_prepare,
#ifdef HAVE_UNITTEST
	.test_group = TEST_GROUP_EVENTS, .test = test_signal,
#endif
};
REGISTER_RAS_EVENT(ras_signal_event);
#include "modules/ras-report.h"

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
	const char *code = "Unknown";
	const char *result = "Unknown";
	const char *message = "Unknown";

	if (ev->code >= 0 && ev->code <= BUS_MCEERR_AO &&
	    errcode_str[ev->code])
		code = errcode_str[ev->code];
	if (ev->result >= 0 && ev->result <= TRACE_SIGNAL_LOSE_INFO &&
	    signal_res[ev->result])
		result = signal_res[ev->result];
	if (ev->sig == SIGBUS && ev->code >= 0 &&
	    ev->code <= BUS_MCEERR_AO && signal_msg[ev->code])
		message = signal_msg[ev->code];

	trace_seq_printf(s,
			 "%s signal: %s, errorno: %d, code: %s, comm: %s, pid: %d, grp: %d, res: %s, msg: %s",
			 ev->timestamp, strsignal(ev->sig), ev->error_no,
			 code,
			 ev->comm, ev->pid,
			 ev->group,
			 result, message);
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

	/* Store data into SQL DB */
	db_signal_event(ras, &ev);


#ifdef HAVE_ABRT_REPORT
	/* Report event to ABRT */
	ras_report_signal_event(ras, &ev);
#endif

	return 0;
}
static const struct db_fields signal_event_fields[] = {
	{ .name = "id",		.type = DB_TYPE_SERIAL, .is_pk = true },
	{ .name = "timestamp",	.type = DB_TYPE_TIMESTAMP, .create_index = true },
	{ .name = "sig",	.type = DB_TYPE_INT32 },
	{ .name = "errorno",	.type = DB_TYPE_INT32 },
	{ .name = "code",	.type = DB_TYPE_INT32 },
	{ .name = "comm",	.type = DB_TYPE_TEXT },
	{ .name = "pid",	.type = DB_TYPE_INT32 },
	{ .name = "grp",	.type = DB_TYPE_INT32 },
	{ .name = "res",	.type = DB_TYPE_INT32 },
};

const struct db_table_descriptor signal_event_tab = {
	.name = "signal_event",
	.fields = signal_event_fields,
	.num_fields = ARRAY_SIZE(signal_event_fields),
};

static struct db_desc_and_stmt signal_event_db = {
	.desc = &signal_event_tab,
};

int db_signal_event(struct ras_events *ras, struct ras_signal_event *ev)
{
	int rc, pos = 1;

	if (!signal_event_db.stmt)
		return -1;
	log(TERM, LOG_INFO, "signal_event store: %p\n", signal_event_db.stmt);

	db_bind(&signal_event_tab, signal_event_db.stmt, pos++, (uint64_t)ev->timestamp, -1);
	db_bind(&signal_event_tab, signal_event_db.stmt, pos++, ev->sig, -1);
	db_bind(&signal_event_tab, signal_event_db.stmt, pos++, ev->error_no, -1);
	db_bind(&signal_event_tab, signal_event_db.stmt, pos++, ev->code, -1);
	db_bind(&signal_event_tab, signal_event_db.stmt, pos++, (uint64_t)ev->comm, -1);
	db_bind(&signal_event_tab, signal_event_db.stmt, pos++, ev->pid, -1);
	db_bind(&signal_event_tab, signal_event_db.stmt, pos++, ev->group, -1);
	db_bind(&signal_event_tab, signal_event_db.stmt, pos++, ev->result, -1);

	rc = db_eval_stmt(signal_event_db.stmt, "signal_event");
	if (!rc)
		log(TERM, LOG_INFO, "register inserted at db\n");

	return rc;
}

static int ras_signal_db_init(struct ras_module_ctx *ctx)
{
	return ras_db_table_register(ctx, &signal_event_db);
}

static void ras_signal_db_cleanup(struct ras_module_ctx *ctx)
{
	ras_db_table_unregister(ctx);
}

static const struct ras_module_entry ras_signal_module = {
	.name = "signal-event",
	.level = BASE_EVENT_MODULE,
	.init = ras_signal_db_init,
	.cleanup = ras_signal_db_cleanup,
};

static void __attribute__((constructor)) ras_signal_register(void)
{
	int rc = module_register(&ras_signal_module);

	if (rc)
		log(TERM, LOG_ERR, "Failed to register signal module: %d\n", rc);
}
