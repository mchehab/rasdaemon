// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * Copyright (C) 2013 Mauro Carvalho Chehab <mchehab+huawei@kernel.org>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <traceevent/kbuffer.h>
#include <unistd.h>

#include "bitfield.h"
#include "ras-aer-handler.h"
#include "ras-logger.h"
#include "ras-report.h"
#include "unified-sel.h"
#include "types.h"

/* bit field meaning for correctable error */
static const char *aer_cor_errors[32] = {
	/* Correctable errors */
	[0]  = "Receiver Error",
	[6]  = "Bad TLP",
	[7]  = "Bad DLLP",
	[8]  = "RELAY_NUM Rollover",
	[12] = "Replay Timer Timeout",
	[13] = "Advisory Non-Fatal",
	[14] = "Corrected Internal Error",
	[15] = "Header Log Overflow",
};

/* bit field meaning for uncorrectable error */
static const char *aer_uncor_errors[32] = {
	/* Uncorrectable errors */
	[4]  = "Data Link Protocol",
	[5]  = "Surprise Link Down",
	[12] = "Poisoned TLP",
	[13] = "Flow Control Protocol",
	[14] = "Completion Timeout",
	[15] = "Completer Abort",
	[16] = "Unexpected Completion",
	[17] = "Receiver Overflow",
	[18] = "Malformed TLP",
	[19] = "ECRC",
	[20] = "Unsupported Request",
	[21] = "ACS Violation",
	[22] = "Uncorrected Internal",
	[23] = "MC Blocked TLP",
	[24] = "AtomicOp Egress Blocked",
	[25] = "TLP Prefix Blocked",
	[26] = "Poisoned TLP Egrees Blocked",
};

static bool use_ipmitool = false;

void ras_aer_handler_init(int enable_ipmitool)
{
#ifdef HAVE_OPENBMC_UNIFIED_SEL
	use_ipmitool = (enable_ipmitool > 0) ? 1 : 0;
#endif
}

#define BUF_LEN	1024

int ras_aer_event_handler(struct trace_seq *s,
			  struct tep_record *record,
			  struct tep_event *event, void *context)
{
	int len;
	unsigned long long severity_val;
	unsigned long long status_val;
	unsigned long long val;
	struct ras_events *ras = context;
	time_t now;
	struct tm *tm;
	struct ras_aer_event ev;
	char buf[BUF_LEN];
#ifdef HAVE_AMP_NS_DECODE
	char ipmi_add_sel[105];
	uint8_t sel_data[5];
	int seg, bus, dev, fn, rc;
#endif
	const char *level;

	if (tep_get_field_val(s, event, "severity", record, &severity_val, 1) < 0)
		return -1;
	switch (severity_val) {
	case HW_EVENT_AER_UNCORRECTED_NON_FATAL:
		level = loglevel_str[LOGLEVEL_CRIT];
		break;
	case HW_EVENT_AER_UNCORRECTED_FATAL:
		level = loglevel_str[LOGLEVEL_EMERG];
		break;
	case HW_EVENT_AER_CORRECTED:
		level = loglevel_str[LOGLEVEL_ERR];
		break;
	default:
		level = loglevel_str[LOGLEVEL_DEBUG];
		break;
	}
	trace_seq_printf(s, "%s ", level);

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

	ev.dev_name = tep_get_field_raw(s, event, "dev_name",
					record, &len, 1);
	if (!ev.dev_name)
		return -1;
	trace_seq_printf(s, "%s ", ev.dev_name);

	if (tep_get_field_val(s,  event, "status", record, &status_val, 1) < 0)
		return -1;

	/* Fills the error buffer. If it is a correctable error then use the
	 * aer_cor_errors bit field. Otherwise use aer_uncor_errors.
	 */
	if (severity_val == HW_EVENT_AER_CORRECTED)
		bitfield_msg(buf, sizeof(buf), aer_cor_errors, 32, 0, 0, status_val);
	else
		bitfield_msg(buf, sizeof(buf), aer_uncor_errors, 32, 0, 0, status_val);
	ev.msg = buf;

	if (tep_get_field_val(s, event, "tlp_header_valid",
			      record, &val, 1) < 0)
		return -1;

	ev.tlp_header_valid = val;
	if (ev.tlp_header_valid) {
		ev.tlp_header = tep_get_field_raw(s, event, "tlp_header",
						  record, &len, 1);
		snprintf((buf + strlen(ev.msg)), BUF_LEN - strlen(ev.msg),
			 " TLP Header: %08x %08x %08x %08x",
			 ev.tlp_header[0], ev.tlp_header[1],
			 ev.tlp_header[2], ev.tlp_header[3]);
	}

	trace_seq_printf(s, "%s ", ev.msg);

	/* Use hw_event_aer_err_type switch between different severity_val */
	switch (severity_val) {
	case HW_EVENT_AER_UNCORRECTED_NON_FATAL:
		ev.error_type = "Uncorrected (Non-Fatal)";
#ifdef HAVE_AMP_NS_DECODE
		sel_data[0] = 0xca;
#endif
		break;
	case HW_EVENT_AER_UNCORRECTED_FATAL:
		ev.error_type = "Uncorrected (Fatal)";
#ifdef HAVE_AMP_NS_DECODE
		sel_data[0] = 0xca;
#endif
		break;
	case HW_EVENT_AER_CORRECTED:
		ev.error_type = "Corrected";
#ifdef HAVE_AMP_NS_DECODE
		sel_data[0] = 0xbf;
#endif
		break;
	default:
		ev.error_type = "Unknown severity";
#ifdef HAVE_AMP_NS_DECODE
		sel_data[0] = 0xbf;
#endif
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

#ifdef HAVE_AMP_NS_DECODE
	/*
	 * Get PCIe AER error source seg/bus/dev/fn and save it into
	 * BMC OEM SEL, ipmitool raw 0x0a 0x44 is IPMI command-Add SEL
	 * entry, please refer IPMI specification chapter 31.6. 0xcd3a
	 * is manufactuer ID(ampere),byte 12 is sensor num(CE is 0xBF,
	 * UE is 0xCA), byte 13~14 is segment number, byte 15 is bus
	 * number, byte 16[7:3] is device number, byte 16[2:0] is
	 * function number
	 */
	rc = sscanf(ev.dev_name, "%x:%x:%x.%x", &seg, &bus, &dev, &fn);
	if (rc == 4) {
		sel_data[1] = seg & 0xff;
		sel_data[2] = (seg & 0xff00) >> 8;
		sel_data[3] = bus;
		sel_data[4] = (((dev & 0x1f) << 3) | (fn & 0x7));

		snprintf(ipmi_add_sel, sizeof(ipmi_add_sel),
			 "ipmitool raw 0x0a 0x44 0x00 0x00 0xc0 0x00 0x00 0x00 0x00 0x3a 0xcd 0x00 0xc0 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x",
			 sel_data[0], sel_data[1], sel_data[2], sel_data[3], sel_data[4]);

		rc = system(ipmi_add_sel);
	}
	if (rc)
		log(SYSLOG, LOG_WARNING, "Failed to execute ipmitool\n");
#endif

#ifdef HAVE_OPENBMC_UNIFIED_SEL
	if (use_ipmitool)
		if (openbmc_unified_sel_log(severity_val, ev.dev_name, status_val) < 0)
			return -1;
#endif

	return 0;
}
