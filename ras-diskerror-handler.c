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
#ifndef __dev_t_defined
#include <sys/types.h>
#endif /* __dev_t_defined */
#include <string.h>
#include <errno.h>
#include <sys/sysmacros.h>
#include "libtrace/kbuffer.h"
#include "ras-diskerror-handler.h"
#include "ras-record.h"
#include "ras-logger.h"
#include "ras-report.h"


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

#define ARRAY_SIZE(x) (sizeof(x)/sizeof(*(x)))

static const char *get_blk_error(int err)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(blk_errors); i++)
		if (blk_errors[i].error == err)
			return blk_errors[i].name;
	return "unknown block error";
}

int ras_diskerror_event_handler(struct trace_seq *s,
				struct pevent_record *record,
				struct event_format *event, void *context)
{
	unsigned long long val;
	int len;
	struct ras_events *ras = context;
	time_t now;
	struct tm *tm;
	struct diskerror_event ev;
	dev_t dev;

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

	if (pevent_get_field_val(s, event, "dev", record, &val, 1) < 0)
		return -1;
	dev = (dev_t)val;
	asprintf(&ev.dev, "%u:%u", major(dev), minor(dev));

	if (pevent_get_field_val(s, event, "sector", record, &val, 1) < 0)
		return -1;
	ev.sector = val;

	if (pevent_get_field_val(s, event, "nr_sector", record, &val, 1) < 0)
		return -1;
	ev.nr_sector = (unsigned int)val;

	if (pevent_get_field_val(s, event, "error", record, &val, 1) < 0)
		return -1;
	ev.error = get_blk_error((int)val);

	ev.rwbs = pevent_get_field_raw(s, event, "rwbs", record, &len, 1);
	if (!ev.rwbs)
		return -1;

	ev.cmd = pevent_get_field_raw(s, event, "cmd", record, &len, 1);
	if (!ev.cmd)
		return -1;

	/* Insert data into the SGBD */
#ifdef HAVE_SQLITE3
	ras_store_diskerror_event(ras, &ev);
#endif

#ifdef HAVE_ABRT_REPORT
	/* Report event to ABRT */
	ras_report_diskerror_event(ras, &ev);
#endif
	free(ev.dev);
	return 0;
}
