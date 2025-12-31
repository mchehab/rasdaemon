// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * Copyright (C) 2019 Cong Wang <xiyou.wangcong@gmail.com>
 */

#define _GNU_SOURCE
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <traceevent/kbuffer.h>

#include "ras-diskerror-handler.h"
#include "ras-logger.h"
#include "ras-report.h"
#include "types.h"

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
