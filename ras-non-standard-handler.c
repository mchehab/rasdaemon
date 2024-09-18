// SPDX-License-Identifier: GPL-2.0

/*
 * Copyright (c) 2016, The Linux Foundation. All rights reserved.
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <traceevent/kbuffer.h>
#include <unistd.h>

#include "ras-logger.h"
#include "ras-non-standard-handler.h"
#include "ras-report.h"
#include "types.h"

static struct  ras_ns_ev_decoder *ras_ns_ev_dec_list;

void print_le_hex(struct trace_seq *s, const uint8_t *buf, int index)
{
	trace_seq_printf(s, "%02x%02x%02x%02x",
			 buf[index + 3], buf[index + 2],
			 buf[index + 1], buf[index]);
}

static char *uuid_le(const char *uu)
{
	static char uuid[sizeof("xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx")];
	char *p = uuid;
	int i;
	static const unsigned char le[16] = {3, 2, 1, 0, 5, 4, 7, 6, 8, 9, 10, 11, 12, 13, 14, 15};

	for (i = 0; i < 16; i++) {
		p += snprintf(p, sizeof(uuid), "%.2x", (unsigned char)uu[le[i]]);
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

int register_ns_ev_decoder(struct ras_ns_ev_decoder *ns_ev_decoder)
{
	struct ras_ns_ev_decoder *list;

	if (!ns_ev_decoder)
		return -1;

	ns_ev_decoder->next = NULL;
#ifdef HAVE_SQLITE3
	ns_ev_decoder->stmt_dec_record = NULL;
#endif
	if (!ras_ns_ev_dec_list) {
		ras_ns_ev_dec_list = ns_ev_decoder;
		ras_ns_ev_dec_list->ref_count = 0;
	} else {
		list = ras_ns_ev_dec_list;
		while (list->next)
			list = list->next;
		list->next = ns_ev_decoder;
	}

	return 0;
}

int ras_ns_add_vendor_tables(struct ras_events *ras)
{
	struct ras_ns_ev_decoder *ns_ev_decoder;
	int error = 0;

#ifdef HAVE_SQLITE3
	if (!ras)
		return -1;

	ns_ev_decoder = ras_ns_ev_dec_list;
	if (ras_ns_ev_dec_list)
		ras_ns_ev_dec_list->ref_count++;
	while (ns_ev_decoder) {
		if (ns_ev_decoder->add_table && !ns_ev_decoder->stmt_dec_record) {
			error = ns_ev_decoder->add_table(ras, ns_ev_decoder);
			if (error)
				break;
		}
		ns_ev_decoder = ns_ev_decoder->next;
	}

	if (error)
		return -1;
#endif

	return 0;
}

static int find_ns_ev_decoder(const char *sec_type, struct ras_ns_ev_decoder **p_ns_ev_dec)
{
	struct ras_ns_ev_decoder *ns_ev_decoder;
	int match = 0;

	ns_ev_decoder = ras_ns_ev_dec_list;
	while (ns_ev_decoder) {
		if (strcmp(uuid_le(sec_type), ns_ev_decoder->sec_type) == 0) {
			*p_ns_ev_dec = ns_ev_decoder;
			match  = 1;
			break;
		}
		ns_ev_decoder = ns_ev_decoder->next;
	}

	if (!match)
		return -1;

	return 0;
}

void ras_ns_finalize_vendor_tables(void)
{
#ifdef HAVE_SQLITE3
	struct ras_ns_ev_decoder *ns_ev_decoder = ras_ns_ev_dec_list;

	if (!ras_ns_ev_dec_list)
		return;

	if (ras_ns_ev_dec_list->ref_count > 0)
		ras_ns_ev_dec_list->ref_count--;
	else
		return;
	if (ras_ns_ev_dec_list->ref_count > 0)
		return;

	while (ns_ev_decoder) {
		if (ns_ev_decoder->stmt_dec_record) {
			ras_mc_finalize_vendor_table(ns_ev_decoder->stmt_dec_record);
			ns_ev_decoder->stmt_dec_record = NULL;
		}
		ns_ev_decoder = ns_ev_decoder->next;
	}
#endif
}

static void unregister_ns_ev_decoder(void)
{
#ifdef HAVE_SQLITE3
	if (!ras_ns_ev_dec_list)
		return;
	ras_ns_ev_dec_list->ref_count = 1;
	ras_ns_finalize_vendor_tables();
#endif
	ras_ns_ev_dec_list = NULL;
}

int ras_non_standard_event_handler(struct trace_seq *s,
				   struct tep_record *record,
				   struct tep_event *event, void *context)
{
	int len, i, line_count;
	unsigned long long val;
	struct ras_events *ras = context;
	time_t now;
	struct tm *tm;
	struct ras_non_standard_event ev;
	struct ras_ns_ev_decoder *ns_ev_decoder;

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

	if (tep_get_field_val(s, event, "sev", record, &val, 1) < 0)
		return -1;
	switch (val) {
	case GHES_SEV_NO:
		ev.severity = "Informational";
		break;
	case GHES_SEV_CORRECTED:
		ev.severity = "Corrected";
		break;
	case GHES_SEV_RECOVERABLE:
		ev.severity = "Recoverable";
		break;
	default:
	case GHES_SEV_PANIC:
		ev.severity = "Fatal";
	}
	trace_seq_printf(s, " %s", ev.severity);

	ev.sec_type = tep_get_field_raw(s, event, "sec_type",
					record, &len, 1);
	if (!ev.sec_type)
		return -1;
	if (strcmp(uuid_le(ev.sec_type),
		   "e8ed898d-df16-43cc-8ecc-54f060ef157f") == 0)
		trace_seq_printf(s, " section type: %s",
				 "Ampere Specific Error");
	else
		trace_seq_printf(s, " section type: %s",
				 uuid_le(ev.sec_type));
	ev.fru_text = tep_get_field_raw(s, event, "fru_text",
					record, &len, 1);
	ev.fru_id = tep_get_field_raw(s, event, "fru_id",
				      record, &len, 1);
	trace_seq_printf(s, " fru text: %s fru id: %s ",
			 ev.fru_text, uuid_le(ev.fru_id));

	if (tep_get_field_val(s, event, "len", record, &val, 1) < 0)
		return -1;
	ev.length = val;
	trace_seq_printf(s, " length: %d", ev.length);

	ev.error = tep_get_field_raw(s, event, "buf", record, &len, 1);
	if (!ev.error)
		return -1;

	if (!find_ns_ev_decoder(ev.sec_type, &ns_ev_decoder)) {
		ns_ev_decoder->decode(ras, ns_ev_decoder, s, &ev);
	} else {
		len = ev.length;
		i = 0;
		line_count = 0;
		trace_seq_printf(s, " error:\n  %08x: ", i);
		while (len >= 4) {
			print_le_hex(s, ev.error, i);
			i += 4;
			len -= 4;
			if (++line_count == 4) {
				trace_seq_printf(s, "\n  %08x: ", i);
				line_count = 0;
			} else {
				trace_seq_printf(s, " ");
			}
		}
	}

	/* Insert data into the SGBD */
#ifdef HAVE_SQLITE3
	ras_store_non_standard_record(ras, &ev);
#endif

#ifdef HAVE_ABRT_REPORT
	/* Report event to ABRT */
	ras_report_non_standard_event(ras, &ev);
#endif

	return 0;
}

__attribute__((destructor))
static void ns_exit(void)
{
	unregister_ns_ev_decoder();
}
