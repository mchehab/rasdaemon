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
#include <traceevent/kbuffer.h>
#include "ras-arm-handler.h"
#include "ras-record.h"
#include "ras-logger.h"
#include "ras-report.h"
#include "ras-non-standard-handler.h"
#include "non-standard-ampere.h"
#include "ras-cpu-isolation.h"

#define ARM_ERR_VALID_ERROR_COUNT BIT(0)
#define ARM_ERR_VALID_FLAGS BIT(1)
#define BIT2 2

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

#ifdef HAVE_CPU_FAULT_ISOLATION
static int is_core_failure(struct ras_arm_err_info *err_info)
{
	if (err_info->validation_bits & ARM_ERR_VALID_FLAGS) {
		/*
		 * core failure:
		 * Bit 0\1\3: (at lease 1)
		 * Bit 2: 0
		 */
		return (err_info->flags & 0xf) && !(err_info->flags & (0x1 << BIT2));
	}
	return 0;
}

static int count_errors(struct ras_arm_event *ev, int sev)
{
	struct ras_arm_err_info *err_info;
	int num_pei;
	int err_info_size = sizeof(struct ras_arm_err_info);
	int num = 0;
	int i;
	int error_count;

	if (ev->pei_len % err_info_size != 0) {
		log(TERM, LOG_ERR,
			"The event data does not match to the ARM Processor Error Information Structure\n");
		return num;
	}
	num_pei = ev->pei_len / err_info_size;
	err_info = (struct ras_arm_err_info *)(ev->pei_error);

	for (i = 0; i < num_pei; ++i) {
		error_count = 1;
		if (err_info->validation_bits & ARM_ERR_VALID_ERROR_COUNT) {
			/*
			 * The value of this field is defined as follows:
			 * 0: Single Error
			 * 1: Multiple Errors
			 * 2-65535: Error Count
			 */
			error_count = err_info->multiple_error + 1;
		}
		if (sev == GHES_SEV_RECOVERABLE && !is_core_failure(err_info))
			error_count = 0;

		num += error_count;
		err_info += 1;
	}
	log(TERM, LOG_INFO, "%d error in cpu core catched\n", num);
	return num;
}

static int ras_handle_cpu_error(struct trace_seq *s,
				struct tep_record *record,
				struct tep_event *event,
				struct ras_arm_event *ev, time_t now)
{
	unsigned long long val;
	int cpu;
	char *severity;
	struct error_info err_info;

	if (tep_get_field_val(s, event, "cpu", record, &val, 1) < 0)
		return -1;
	cpu = val;
	trace_seq_printf(s, "\n cpu: %d", cpu);

	/* record cpu error */
	if (tep_get_field_val(s, event, "sev", record, &val, 1) < 0)
		return -1;
	/* refer to UEFI_2_9 specification chapter N2.2 Table N-5 */
	switch (val) {
	case GHES_SEV_NO:
		severity = "Informational";
		break;
	case GHES_SEV_CORRECTED:
		severity = "Corrected";
		break;
	case GHES_SEV_RECOVERABLE:
		severity = "Recoverable";
		break;
	default:
	case GHES_SEV_PANIC:
		severity = "Fatal";
	}
	trace_seq_printf(s, "\n severity: %s", severity);

	if (val == GHES_SEV_CORRECTED || val == GHES_SEV_RECOVERABLE) {
		int nums = count_errors(ev, val);

		if (nums > 0) {
			err_info.nums = nums;
			err_info.time = now;
			err_info.err_type = val;
			ras_record_cpu_error(&err_info, cpu);
		}
	}

	return 0;
}
#endif

int ras_arm_event_handler(struct trace_seq *s,
			  struct tep_record *record,
			  struct tep_event *event, void *context)
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

	if (tep_get_field_val(s, event, "affinity", record, &val, 1) < 0)
		return -1;
	ev.affinity = val;
	trace_seq_printf(s, " affinity: %d", ev.affinity);

	if (tep_get_field_val(s, event, "mpidr", record, &val, 1) < 0)
		return -1;
	ev.mpidr = val;
	trace_seq_printf(s, "\n MPIDR: 0x%llx", (unsigned long long)ev.mpidr);

	if (tep_get_field_val(s, event, "midr", record, &val, 1) < 0)
		return -1;
	ev.midr = val;
	trace_seq_printf(s, "\n MIDR: 0x%llx", (unsigned long long)ev.midr);

	if (tep_get_field_val(s, event, "running_state", record, &val, 1) < 0)
		return -1;
	ev.running_state = val;
	trace_seq_printf(s, "\n running_state: %d", ev.running_state);

	if (tep_get_field_val(s, event, "psci_state", record, &val, 1) < 0)
		return -1;
	ev.psci_state = val;
	trace_seq_printf(s, "\n psci_state: %d", ev.psci_state);

	if (tep_get_field_val(s, event, "pei_len", record, &val, 1) < 0)
		return -1;
	ev.pei_len = val;
	trace_seq_printf(s, "\n ARM Processor Err Info data len: %d\n",
			 ev.pei_len);

	ev.pei_error = tep_get_field_raw(s, event, "buf", record, &len, 1);
	if (!ev.pei_error)
		return -1;
	display_raw_data(s, ev.pei_error, ev.pei_len);

	if (tep_get_field_val(s, event, "ctx_len", record, &val, 1) < 0)
		return -1;
	ev.ctx_len = val;
	trace_seq_printf(s, "\n ARM Processor Err Context Info data len: %d\n",
			 ev.ctx_len);

	ev.ctx_error = tep_get_field_raw(s, event, "buf1", record, &len, 1);
	if (!ev.ctx_error)
		return -1;
	display_raw_data(s, ev.ctx_error, ev.ctx_len);

	if (tep_get_field_val(s, event, "oem_len", record, &val, 1) < 0)
		return -1;
	ev.oem_len = val;
	trace_seq_printf(s, "\n Vendor Specific Err Info data len: %d\n",
			 ev.oem_len);

	ev.vsei_error = tep_get_field_raw(s, event, "buf2", record, &len, 1);
	if (!ev.vsei_error)
		return -1;

#ifdef HAVE_AMP_NS_DECODE
	//decode ampere specific error
	decode_amp_payload0_err_regs(NULL, s,
				(struct amp_payload0_type_sec *)ev.vsei_error);
#else
	display_raw_data(s, ev.vsei_error, ev.oem_len);
#endif

#ifdef HAVE_CPU_FAULT_ISOLATION
	if (ras_handle_cpu_error(s, record, event, &ev, now) < 0)
		return -1;
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
