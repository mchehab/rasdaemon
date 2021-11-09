/*
 * Copyright (C) 2013 Mauro Carvalho Chehab <mchehab+redhat@kernel.org>
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
#ifdef HAVE_AMP_NS_DECODE
#include <stdbool.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#endif

/* bit field meaning for correctable error */
static const char *aer_cor_errors[32] = {
	/* Correctable errors */
	[0]  = "Receiver Error",
	[6]  = "Bad TLP",
	[7]  = "Bad DLLP",
	[8]  = "RELAY_NUM Rollover",
	[12] = "Replay Timer Timeout",
	[13] = "Advisory Non-Fatal",
};

/* bit field meaning for uncorrectable error */
static const char *aer_uncor_errors[32] = {
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

#ifdef HAVE_AMP_NS_DECODE
#define IPMITOOL_CMD "/usr/bin/ipmitool"
#define DMIDECODE_CMD "/usr/sbin/dmidecode"
static bool ampere_ipmitool = false;

static void ras_report_aer_ipmi_init(void)
{
	struct utsname unm;
	struct stat st;
	int rc;

	/*
	 * Verify on startup if we are on an Ampere Altra or Altra Max
	 * platform, to set the use of ipmitool (if installed).
	 */
	if (stat(IPMITOOL_CMD, &st) != 0)
		return;

	if ((uname(&unm) != 0) || (strncmp(unm.machine, "aarch64", 8) != 0))
		return;

	/* prefer dmidecode as lscpu may not have the necessary dmi info */
	if (stat(DMIDECODE_CMD, &st) == 0)
		rc = system(DMIDECODE_CMD" -t 4 | /usr/bin/grep "
		    "'Ampere(R) Altra(R)' > /dev/null");
	else
		rc = system("/usr/bin/lscpu | /usr/bin/grep "
		    "'Ampere(R) Altra(R)' > /dev/null");
	if (rc == -1 || !WIFEXITED(rc) || WEXITSTATUS(rc))
		return;

	ampere_ipmitool = true;
}

static void ras_report_aer_ipmi(int severity_val, struct ras_aer_event *ev)
{
	char ipmi_add_sel[114];
	uint8_t sel_data[5];
	int seg, bus, dev, fn, rc;

	if (!ampere_ipmitool)
		return;

	/*
	 * Get PCIe AER error source seg/bus/dev/fn and save it into
	 * BMC OEM SEL, ipmitool raw 0x0a 0x44 is IPMI command-Add SEL
	 * entry, please refer IPMI specification chapter 31.6. 0xcd3a
	 * is manufactuer ID(ampere),byte 12 is sensor num(CE is 0xBF,
	 * UE is 0xCA), byte 13~14 is segment number, byte 15 is bus
	 * number, byte 16[7:3] is device number, byte 16[2:0] is
	 * function number.
	 */

	switch (severity_val) {
	case HW_EVENT_AER_UNCORRECTED_NON_FATAL:
	case HW_EVENT_AER_UNCORRECTED_FATAL:
		sel_data[0] = 0xca;
		break;
	case HW_EVENT_AER_CORRECTED:
	default:
		sel_data[0] = 0xbf;
	}

	sscanf(ev->dev_name, "%x:%x:%x.%x", &seg, &bus, &dev, &fn);

	sel_data[1] = seg & 0xff;
	sel_data[2] = (seg & 0xff00) >> 8;
	sel_data[3] = bus;
	sel_data[4] = (((dev & 0x1f) << 3) | (fn & 0x7));

	sprintf(ipmi_add_sel, IPMITOOL_CMD
	  " raw 0x0a 0x44 0x00 0x00 0xc0 0x00 0x00 0x00 0x00 0x3a 0xcd 0x00 0xc0 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x",
	  sel_data[0], sel_data[1], sel_data[2], sel_data[3], sel_data[4]);

	rc = system(ipmi_add_sel);
	if (rc == -1 || !WIFEXITED(rc) || WEXITSTATUS(rc))
		log(TERM, LOG_ERR, "ipmitool command failed [%d]", rc);
}
#endif

#define BUF_LEN	1024

int ras_aer_event_handler(struct trace_seq *s,
			 struct pevent_record *record,
			 struct event_format *event, void *context)
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
	trace_seq_printf(s, "%s ", ev.dev_name);

	if (pevent_get_field_val(s,  event, "status", record, &status_val, 1) < 0)
		return -1;

	if (pevent_get_field_val(s, event, "severity", record, &severity_val, 1) < 0)
		return -1;

	/* Fills the error buffer. If it is a correctable error then use the
	 * aer_cor_errors bit field. Otherwise use aer_uncor_errors.
	 */
	if (severity_val == HW_EVENT_AER_CORRECTED)
		bitfield_msg(buf, sizeof(buf), aer_cor_errors, 32, 0, 0, status_val);
	else
		bitfield_msg(buf, sizeof(buf), aer_uncor_errors, 32, 0, 0, status_val);
	ev.msg = buf;

	if (pevent_get_field_val(s, event, "tlp_header_valid",
				record, &val, 1) < 0)
		return -1;

	ev.tlp_header_valid = val;
	if (ev.tlp_header_valid) {
		ev.tlp_header = pevent_get_field_raw(s, event, "tlp_header",
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
		break;
	case HW_EVENT_AER_UNCORRECTED_FATAL:
		ev.error_type = "Uncorrected (Fatal)";
		break;
	case HW_EVENT_AER_CORRECTED:
		ev.error_type = "Corrected";
		break;
	default:
		ev.error_type = "Unknown severity";
	}
	trace_seq_puts(s, ev.error_type);

#ifdef HAVE_SQLITE3
	/* Insert data into the SGBD */
	ras_store_aer_event(ras, &ev);
#endif

#ifdef HAVE_ABRT_REPORT
	/* Report event to ABRT */
	ras_report_aer_event(ras, &ev);
#endif

#ifdef HAVE_AMP_NS_DECODE
	/* Give a chance to provide AER error though IPMI */
	ras_report_aer_ipmi(severity_val, &ev);
#endif

	return 0;
}

void ras_aer_handler_init(void)
{
#ifdef HAVE_AMP_NS_DECODE
	ras_report_aer_ipmi_init();
#endif
}
