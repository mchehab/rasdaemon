/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
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
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <traceevent/kbuffer.h>
#include "ras-cxl-handler.h"
#include "ras-record.h"
#include "ras-logger.h"
#include "ras-report.h"

/* Poison List: Payload out flags */
#define CXL_POISON_FLAG_MORE            BIT(0)
#define CXL_POISON_FLAG_OVERFLOW        BIT(1)
#define CXL_POISON_FLAG_SCANNING        BIT(2)

/* CXL poison - source types */
enum cxl_poison_source {
	CXL_POISON_SOURCE_UNKNOWN = 0,
	CXL_POISON_SOURCE_EXTERNAL = 1,
	CXL_POISON_SOURCE_INTERNAL = 2,
	CXL_POISON_SOURCE_INJECTED = 3,
	CXL_POISON_SOURCE_VENDOR = 7,
};

/* CXL poison - trace types */
enum cxl_poison_trace_type {
	CXL_POISON_TRACE_LIST,
	CXL_POISON_TRACE_INJECT,
	CXL_POISON_TRACE_CLEAR,
};

int ras_cxl_poison_event_handler(struct trace_seq *s,
				 struct tep_record *record,
				 struct tep_event *event, void *context)
{
	int len;
	unsigned long long val;
	struct ras_events *ras = context;
	time_t now;
	struct tm *tm;
	struct ras_cxl_poison_event ev;

	now = record->ts / user_hz + ras->uptime_diff;
	tm = localtime(&now);
	if (tm)
		strftime(ev.timestamp, sizeof(ev.timestamp),
			 "%Y-%m-%d %H:%M:%S %z", tm);
	else
		strncpy(ev.timestamp, "1970-01-01 00:00:00 +0000", sizeof(ev.timestamp));
	if (trace_seq_printf(s, "%s ", ev.timestamp) <= 0)
		return -1;

	ev.memdev = tep_get_field_raw(s, event, "memdev",
				      record, &len, 1);
	if (!ev.memdev)
		return -1;
	if (trace_seq_printf(s, "memdev:%s ", ev.memdev) <= 0)
		return -1;

	ev.host = tep_get_field_raw(s, event, "host",
				    record, &len, 1);
	if (!ev.host)
		return -1;
	if (trace_seq_printf(s, "host:%s ", ev.host) <= 0)
		return -1;

	if (tep_get_field_val(s, event, "serial", record, &val, 1) < 0)
		return -1;
	ev.serial = val;
	if (trace_seq_printf(s, "serial:0x%llx ", (unsigned long long)ev.serial) <= 0)
		return -1;

	if (tep_get_field_val(s,  event, "trace_type", record, &val, 1) < 0)
		return -1;
	switch (val) {
	case CXL_POISON_TRACE_LIST:
		ev.trace_type = "List";
		break;
	case CXL_POISON_TRACE_INJECT:
		ev.trace_type = "Inject";
		break;
	case CXL_POISON_TRACE_CLEAR:
		ev.trace_type = "Clear";
		break;
	default:
		ev.trace_type = "Invalid";
	}
	if (trace_seq_printf(s, "trace_type:%s ", ev.trace_type) <= 0)
		return -1;

	ev.region = tep_get_field_raw(s, event, "region",
				      record, &len, 1);
	if (!ev.region)
		return -1;
	if (trace_seq_printf(s, "region:%s ", ev.region) <= 0)
		return -1;

	ev.uuid = tep_get_field_raw(s, event, "uuid",
				    record, &len, 1);
	if (!ev.uuid)
		return -1;
	if (trace_seq_printf(s, "region_uuid:%s ", ev.uuid) <= 0)
		return -1;

	if (tep_get_field_val(s, event, "hpa", record, &val, 1) < 0)
		return -1;
	ev.hpa = val;
	if (trace_seq_printf(s, "poison list: hpa:0x%llx ", (unsigned long long)ev.hpa) <= 0)
		return -1;

	if (tep_get_field_val(s, event, "dpa", record, &val, 1) < 0)
		return -1;
	ev.dpa = val;
	if (trace_seq_printf(s, "dpa:0x%llx ", (unsigned long long)ev.dpa) <= 0)
		return -1;

	if (tep_get_field_val(s, event, "dpa_length", record, &val, 1) < 0)
		return -1;
	ev.dpa_length = val;
	if (trace_seq_printf(s, "dpa_length:0x%x ", ev.dpa_length) <= 0)
		return -1;

	if (tep_get_field_val(s,  event, "source", record, &val, 1) < 0)
		return -1;
	switch (val) {
	case CXL_POISON_SOURCE_UNKNOWN:
		ev.source = "Unknown";
		break;
	case CXL_POISON_SOURCE_EXTERNAL:
		ev.source = "External";
		break;
	case CXL_POISON_SOURCE_INTERNAL:
		ev.source = "Internal";
		break;
	case CXL_POISON_SOURCE_INJECTED:
		ev.source = "Injected";
		break;
	case CXL_POISON_SOURCE_VENDOR:
		ev.source = "Vendor";
		break;
	default:
		ev.source = "Invalid";
	}
	if (trace_seq_printf(s, "source:%s ", ev.source) <= 0)
		return -1;

	if (tep_get_field_val(s,  event, "flags", record, &val, 1) < 0)
		return -1;
	ev.flags = val;
	if (trace_seq_printf(s, "flags:%d ", ev.flags) <= 0)
		return -1;

	if (ev.flags & CXL_POISON_FLAG_OVERFLOW) {
		if (tep_get_field_val(s,  event, "overflow_ts", record, &val, 1) < 0)
			return -1;
		if (val) {
			/* CXL Specification 3.0
			 * Overflow timestamp - The number of unsigned nanoseconds
			 * that have elapsed since midnight, 01-Jan-1970 UTC
			 */
			time_t ovf_ts_secs = val / 1000000000ULL;

			tm = localtime(&ovf_ts_secs);
			if (tm) {
				strftime(ev.overflow_ts, sizeof(ev.overflow_ts),
					 "%Y-%m-%d %H:%M:%S %z", tm);
			}
		}
		if (!val || !tm)
			strncpy(ev.overflow_ts, "1970-01-01 00:00:00 +0000",
				sizeof(ev.overflow_ts));
	} else
		strncpy(ev.overflow_ts, "1970-01-01 00:00:00 +0000", sizeof(ev.overflow_ts));
	if (trace_seq_printf(s, "overflow timestamp:%s\n", ev.overflow_ts) <= 0)
		return -1;

	/* Insert data into the SGBD */
#ifdef HAVE_SQLITE3
	ras_store_cxl_poison_event(ras, &ev);
#endif

#ifdef HAVE_ABRT_REPORT
	/* Report event to ABRT */
	ras_report_cxl_poison_event(ras, &ev);
#endif

	return 0;
}
