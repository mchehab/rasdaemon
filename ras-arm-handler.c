/*
 * Copyright (c) 2016, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "libtrace/kbuffer.h"
#include "ras-arm-handler.h"
#include "ras-record.h"
#include "ras-logger.h"
#include "ras-report.h"
#include "ras-non-standard-handler.h"
#include "non-standard-ampere.h"

void display_raw_data(struct trace_seq *s,
		const uint8_t *buf,
		uint32_t datalen)
{
	int i = 0, line_count = 0;

	trace_seq_printf(s, "  %08x: ", i);
	while (datalen >= 4) {
		print_le_hex(s, buf, i);
		i += 4;
		datalen -= 4;
		if (++line_count == 4) {
			trace_seq_printf(s, "\n  %08x: ", i);
			line_count = 0;
		} else
			trace_seq_printf(s, " ");
	}
}

int ras_arm_event_handler(struct trace_seq *s,
			 struct pevent_record *record,
			 struct event_format *event, void *context)
{
	unsigned long long val;
	struct ras_events *ras = context;
	time_t now;
	struct tm *tm;
	struct ras_arm_event ev;
	int len = 0;
	memset(&ev, 0, sizeof(ev));

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
	trace_seq_printf(s, "%s\n", ev.timestamp);

	if (pevent_get_field_val(s, event, "affinity", record, &val, 1) < 0)
		return -1;
	ev.affinity = val;
	trace_seq_printf(s, " affinity: %d", ev.affinity);

	if (pevent_get_field_val(s, event, "mpidr", record, &val, 1) < 0)
		return -1;
	ev.mpidr = val;
	trace_seq_printf(s, "\n MPIDR: 0x%llx", (unsigned long long)ev.mpidr);

	if (pevent_get_field_val(s, event, "midr", record, &val, 1) < 0)
		return -1;
	ev.midr = val;
	trace_seq_printf(s, "\n MIDR: 0x%llx", (unsigned long long)ev.midr);

	if (pevent_get_field_val(s, event, "running_state", record, &val, 1) < 0)
		return -1;
	ev.running_state = val;
	trace_seq_printf(s, "\n running_state: %d", ev.running_state);

	if (pevent_get_field_val(s, event, "psci_state", record, &val, 1) < 0)
		return -1;
	ev.psci_state = val;
	trace_seq_printf(s, "\n psci_state: %d", ev.psci_state);

	if (pevent_get_field_val(s, event, "pei_len", record, &val, 1) < 0)
		return -1;
	ev.pei_len = val;
	trace_seq_printf(s, "\n ARM Processor Err Info data len: %d\n",
			 ev.pei_len);

	ev.pei_error = pevent_get_field_raw(s, event, "buf", record, &len, 1);
	if (!ev.pei_error)
		return -1;
	display_raw_data(s, ev.pei_error, ev.pei_len);

	if (pevent_get_field_val(s, event, "ctx_len", record, &val, 1) < 0)
		return -1;
	ev.ctx_len = val;
	trace_seq_printf(s, "\n ARM Processor Err Context Info data len: %d\n",
			 ev.ctx_len);

	ev.ctx_error = pevent_get_field_raw(s, event, "buf1", record, &len, 1);
	if (!ev.ctx_error)
		return -1;
	display_raw_data(s, ev.ctx_error, ev.ctx_len);

	if (pevent_get_field_val(s, event, "oem_len", record, &val, 1) < 0)
		return -1;
	ev.oem_len = val;
	trace_seq_printf(s, "\n Vendor Specific Err Info data len: %d\n",
			 ev.oem_len);

	ev.vsei_error = pevent_get_field_raw(s, event, "buf2", record, &len, 1);
	if (!ev.vsei_error)
		return -1;

#ifdef HAVE_AMP_NS_DECODE
	//decode ampere specific error
	decode_amp_payload0_err_regs(NULL, s,
				(struct amp_payload0_type_sec *)ev.vsei_error);

#else
	display_raw_data(s, ev.vsei_error, ev.oem_len);

#endif

	/* Insert data into the SGBD */
#ifdef HAVE_SQLITE3
	ras_store_arm_record(ras, &ev);
#endif

#ifdef HAVE_ABRT_REPORT
	/* Report event to ABRT */
	ras_report_arm_event(ras, &ev);
#endif

	return 0;
}
