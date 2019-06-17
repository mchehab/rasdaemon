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
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include "libtrace/kbuffer.h"
#include "ras-non-standard-handler.h"
#include "ras-record.h"
#include "ras-logger.h"
#include "ras-report.h"

static p_ns_dec_tab * ns_dec_tab;
static size_t dec_tab_count;

int register_ns_dec_tab(const p_ns_dec_tab tab)
{
	ns_dec_tab = (p_ns_dec_tab *)realloc(ns_dec_tab,
					    (dec_tab_count + 1) * sizeof(tab));
	if (ns_dec_tab == NULL) {
		printf("%s p_ns_dec_tab malloc failed", __func__);
		return -1;
	}
	ns_dec_tab[dec_tab_count] = tab;
	dec_tab_count++;
	return 0;
}

void unregister_ns_dec_tab(void)
{
	if (ns_dec_tab) {
		free(ns_dec_tab);
		ns_dec_tab = NULL;
		dec_tab_count = 0;
	}
}

void print_le_hex(struct trace_seq *s, const uint8_t *buf, int index) {
	trace_seq_printf(s, "%02x%02x%02x%02x", buf[index+3], buf[index+2], buf[index+1], buf[index]);
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

static int uuid_le_cmp(const char *sec_type, const char *uuid2)
{
	static char uuid1[32];
	char *p = uuid1;
	int i;
	static const unsigned char le[16] = {
			3, 2, 1, 0, 5, 4, 7, 6, 8, 9, 10, 11, 12, 13, 14, 15};

	for (i = 0; i < 16; i++)
		p += sprintf(p, "%.2x", sec_type[le[i]]);
	*p = 0;
	return strncmp(uuid1, uuid2, 32);
}

int ras_non_standard_event_handler(struct trace_seq *s,
			 struct pevent_record *record,
			 struct event_format *event, void *context)
{
	int len, i, line_count, count;
	unsigned long long val;
	struct ras_events *ras = context;
	time_t now;
	struct tm *tm;
	struct ras_non_standard_event ev;
	p_ns_dec_tab dec_tab;
	bool dec_done = false;

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

	if (pevent_get_field_val(s, event, "sev", record, &val, 1) < 0)
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
	trace_seq_printf(s, "\n %s", ev.severity);

	ev.sec_type = pevent_get_field_raw(s, event, "sec_type", record, &len, 1);
	if(!ev.sec_type)
		return -1;
	trace_seq_printf(s, "\n section type: %s", uuid_le(ev.sec_type));
	ev.fru_text = pevent_get_field_raw(s, event, "fru_text",
						record, &len, 1);
	ev.fru_id = pevent_get_field_raw(s, event, "fru_id",
						record, &len, 1);
	trace_seq_printf(s, " fru text: %s fru id: %s ",
				ev.fru_text,
				uuid_le(ev.fru_id));

	if (pevent_get_field_val(s, event, "len", record, &val, 1) < 0)
		return -1;
	ev.length = val;
	trace_seq_printf(s, "\n length: %d\n", ev.length);

	ev.error = pevent_get_field_raw(s, event, "buf", record, &len, 1);
	if(!ev.error)
		return -1;

	for (count = 0; count < dec_tab_count && !dec_done; count++) {
		dec_tab = ns_dec_tab[count];
		for (i = 0; dec_tab[i].decode; i++) {
			if (uuid_le_cmp(ev.sec_type,
					dec_tab[i].sec_type) == 0) {
				dec_tab[i].decode(ras, &dec_tab[i],
						  s, ev.error);
				dec_done = true;
				break;
			}
		}
	}

	if (!dec_done) {
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
			} else
				trace_seq_printf(s, " ");
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
	unregister_ns_dec_tab();
}
