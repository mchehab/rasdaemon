/*
 * Copyright (C) 2014 Tony Luck <tony.luck@intel.com>
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
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include "libtrace/kbuffer.h"
#include "ras-extlog-handler.h"
#include "ras-record.h"
#include "ras-logger.h"
#include "ras-report.h"

static char *err_type(int etype)
{
	switch (etype) {
	case 0: return "unknown";
	case 1: return "no error";
	case 2: return "single-bit ECC";
	case 3: return "multi-bit ECC";
	case 4: return "single-symbol chipkill ECC";
	case 5: return "multi-symbol chipkill ECC";
	case 6: return "master abort";
	case 7: return "target abort";
	case 8: return "parity error";
	case 9: return "watchdog timeout";
	case 10: return "invalid address";
	case 11: return "mirror Broken";
	case 12: return "memory sparing";
	case 13: return "scrub corrected error";
	case 14: return "scrub uncorrected error";
	case 15: return "physical memory map-out event";
	}
	return "unknown-type";
}

static char *err_severity(int severity)
{
	switch (severity) {
	case 0: return "recoverable";
	case 1: return "fatal";
	case 2: return "corrected";
	case 3: return "informational";
	}
	return "unknown-severity";
}

static unsigned long long err_mask(int lsb)
{
	if (lsb == 0xff)
		return ~0ull;
	return ~((1ull << lsb) - 1);
}

#define CPER_MEM_VALID_NODE			0x0008
#define CPER_MEM_VALID_CARD			0x0010
#define CPER_MEM_VALID_MODULE			0x0020
#define CPER_MEM_VALID_BANK			0x0040
#define CPER_MEM_VALID_DEVICE			0x0080
#define CPER_MEM_VALID_ROW			0x0100
#define CPER_MEM_VALID_COLUMN			0x0200
#define CPER_MEM_VALID_BIT_POSITION		0x0400
#define CPER_MEM_VALID_REQUESTOR_ID		0x0800
#define CPER_MEM_VALID_RESPONDER_ID		0x1000
#define CPER_MEM_VALID_TARGET_ID		0x2000
#define CPER_MEM_VALID_RANK_NUMBER		0x8000
#define CPER_MEM_VALID_CARD_HANDLE		0x10000
#define CPER_MEM_VALID_MODULE_HANDLE		0x20000

struct cper_mem_err_compact {
	unsigned long long	validation_bits;
	unsigned short		node;
	unsigned short		card;
	unsigned short		module;
	unsigned short		bank;
	unsigned short		device;
	unsigned short		row;
	unsigned short		column;
	unsigned short		bit_pos;
	unsigned long long	requestor_id;
	unsigned long long	responder_id;
	unsigned long long	target_id;
	unsigned short		rank;
	unsigned short		mem_array_handle;
	unsigned short		mem_dev_handle;
};

static char *err_cper_data(const char *c)
{
	const struct cper_mem_err_compact *cpd = (struct cper_mem_err_compact *)c;
	static char buf[256];
	char *p = buf;

	if (cpd->validation_bits == 0)
		return "";
	p += sprintf(p, " (");
	if (cpd->validation_bits & CPER_MEM_VALID_NODE)
		p += sprintf(p, "node: %d ", cpd->node);
	if (cpd->validation_bits & CPER_MEM_VALID_CARD)
		p += sprintf(p, "card: %d ", cpd->card);
	if (cpd->validation_bits & CPER_MEM_VALID_MODULE)
		p += sprintf(p, "module: %d ", cpd->module);
	if (cpd->validation_bits & CPER_MEM_VALID_BANK)
		p += sprintf(p, "bank: %d ", cpd->bank);
	if (cpd->validation_bits & CPER_MEM_VALID_DEVICE)
		p += sprintf(p, "device: %d ", cpd->device);
	if (cpd->validation_bits & CPER_MEM_VALID_ROW)
		p += sprintf(p, "row: %d ", cpd->row);
	if (cpd->validation_bits & CPER_MEM_VALID_COLUMN)
		p += sprintf(p, "column: %d ", cpd->column);
	if (cpd->validation_bits & CPER_MEM_VALID_BIT_POSITION)
		p += sprintf(p, "bit_pos: %d ", cpd->bit_pos);
	if (cpd->validation_bits & CPER_MEM_VALID_REQUESTOR_ID)
		p += sprintf(p, "req_id: 0x%llx ", cpd->requestor_id);
	if (cpd->validation_bits & CPER_MEM_VALID_RESPONDER_ID)
		p += sprintf(p, "resp_id: 0x%llx ", cpd->responder_id);
	if (cpd->validation_bits & CPER_MEM_VALID_TARGET_ID)
		p += sprintf(p, "tgt_id: 0x%llx ", cpd->target_id);
	if (cpd->validation_bits & CPER_MEM_VALID_RANK_NUMBER)
		p += sprintf(p, "rank: %d ", cpd->rank);
	if (cpd->validation_bits & CPER_MEM_VALID_CARD_HANDLE)
		p += sprintf(p, "card_handle: %d ", cpd->mem_array_handle);
	if (cpd->validation_bits & CPER_MEM_VALID_MODULE_HANDLE)
		p += sprintf(p, "module_handle: %d ", cpd->mem_dev_handle);
	p += sprintf(p-1, ")");

	return buf;
}

static char *uuid_le(const char *uu)
{
	static char uuid[sizeof("xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx")];
	char *p = uuid;
	int i;
	static const unsigned char le[16] = {3,2,1,0,5,4,7,6,8,9,10,11,12,13,14,15};

	for (i = 0; i < 16; i++) {
		p += sprintf(p, "%.2x", uu[le[i]]);
		switch (i) {
		case 3:
		case 5:
		case 7:
		case 9:
			*p++ = '-';
			break;
		}
	}

	*p = 0;

	return uuid;
}


static void report_extlog_mem_event(struct ras_events *ras,
				    struct pevent_record *record,
				    struct trace_seq *s,
				    struct ras_extlog_event *ev)
{
	trace_seq_printf(s, "%d %s error: %s physical addr: 0x%llx mask: 0x%llx%s %s %s",
		ev->error_seq, err_severity(ev->severity),
		err_type(ev->etype), ev->address,
		err_mask(ev->pa_mask_lsb),
		err_cper_data(ev->cper_data),
		ev->fru_text,
		uuid_le(ev->fru_id));
}

int ras_extlog_mem_event_handler(struct trace_seq *s,
			  struct pevent_record *record,
			  struct event_format *event, void *context)
{
	int len;
	unsigned long long val;
	struct ras_events *ras = context;
	time_t now;
	struct tm *tm;
	struct ras_extlog_event ev;

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

	if (pevent_get_field_val(s,  event, "etype", record, &val, 1) < 0)
		return -1;
	ev.etype = val;
	if (pevent_get_field_val(s,  event, "err_seq", record, &val, 1) < 0)
		return -1;
	ev.error_seq = val;
	if (pevent_get_field_val(s,  event, "sev", record, &val, 1) < 0)
		return -1;
	ev.severity = val;
	if (pevent_get_field_val(s,  event, "pa", record, &val, 1) < 0)
		return -1;
	ev.address = val;
	if (pevent_get_field_val(s,  event, "pa_mask_lsb", record, &val, 1) < 0)
		return -1;
	ev.pa_mask_lsb = val;

	ev.cper_data = pevent_get_field_raw(s, event, "data",
					   record, &len, 1);
	ev.cper_data_length = len;
	ev.fru_text = pevent_get_field_raw(s, event, "fru_text",
					   record, &len, 1);
	ev.fru_id = pevent_get_field_raw(s, event, "fru_id",
					   record, &len, 1);

	report_extlog_mem_event(ras, record, s, &ev);

	ras_store_extlog_mem_record(ras, &ev);

	return 0;
}
