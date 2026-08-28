// SPDX-License-Identifier: GPL-2.0-only

/*
 * Copyright (c) 2016, The Linux Foundation. All rights reserved.
 */

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <traceevent/kbuffer.h>
#include <unistd.h>

#include "core/modules.h"
#include "core/ras-logger.h"
#include "core/types.h"
#include "db/ras-db.h"
#include "events-arch-arm/ras-non-standard-handler.h"

static int ras_non_standard_event_handler(struct trace_seq *s,
					  struct tep_record *record,
					  struct tep_event *event, void *context);
int db_non_standard_record(struct ras_events *ras, void *priv);

static const struct ras_event_entry ras_non_standard_event_entry = {
	.group = "ras", .event = "non_standard_event",
	.handler = ras_non_standard_event_handler, .id = NON_STANDARD_EVENT,
	.record = db_non_standard_record,
};
REGISTER_RAS_EVENT(ras_non_standard_event_entry);

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
		p += snprintf(p, sizeof(uuid) - (p - uuid), "%.2x", (unsigned char)uu[le[i]]);
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

	if (!ns_ev_decoder || !ns_ev_decoder->sec_type)
		return -EINVAL;

	if (!ras_ns_ev_dec_list) {
		ns_ev_decoder->next = NULL;
		ras_ns_ev_dec_list = ns_ev_decoder;
	} else {
		list = ras_ns_ev_dec_list;
		while (list) {
			if (list == ns_ev_decoder ||
			    !strcmp(list->sec_type, ns_ev_decoder->sec_type))
				return -EEXIST;
			if (!list->next)
				break;
			list = list->next;
		}
		ns_ev_decoder->next = NULL;
		list->next = ns_ev_decoder;
	}

	return 0;
}

void unregister_ns_ev_decoder(struct ras_ns_ev_decoder *ns_ev_decoder)
{
	struct ras_ns_ev_decoder **link = &ras_ns_ev_dec_list;

	if (!ns_ev_decoder)
		return;

	while (*link && *link != ns_ev_decoder)
		link = &(*link)->next;
	if (!*link)
		return;

	*link = ns_ev_decoder->next;
	ns_ev_decoder->next = NULL;
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

#ifdef HAVE_UNITTEST
size_t ras_ns_test_decoder_count(void)
{
	struct ras_ns_ev_decoder *decoder;
	size_t count = 0;

	for (decoder = ras_ns_ev_dec_list; decoder; decoder = decoder->next)
		count++;
	return count;
}

const char *ras_ns_test_decoder_type(size_t index)
{
	struct ras_ns_ev_decoder *decoder = ras_ns_ev_dec_list;

	while (decoder && index--)
		decoder = decoder->next;
	return decoder ? decoder->sec_type : NULL;
}

int ras_ns_test_decode(const char *type, struct ras_events *ras,
		       struct trace_seq *seq,
		       struct ras_non_standard_event *event)
{
	struct ras_ns_ev_decoder *decoder;

	for (decoder = ras_ns_ev_dec_list; decoder; decoder = decoder->next) {
		if (!strcmp(type, decoder->sec_type))
			return decoder->decode ? decoder->decode(ras, decoder, seq,
							 event) : -1;
	}
	return -1;
}
#endif

static int ras_non_standard_event_handler(struct trace_seq *s,
				   struct tep_record *record,
				   struct tep_event *event, void *context)
{
	int len, raw_len, i, line_count, decoded = 0;
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

	ev.error = tep_get_field_raw(s, event, "buf", record, &raw_len, 1);
	if (!ev.error)
		return -1;
	if (ev.length < 0 || ev.length > raw_len) {
		log(TERM, LOG_WARNING,
		    "Ignoring non-standard event with invalid length %d (raw %d)\n",
		    ev.length, raw_len);
		return -1;
	}

	if (!find_ns_ev_decoder(ev.sec_type, &ns_ev_decoder)) {
		if (ns_ev_decoder->decode) {
			ns_ev_decoder->decode(ras, ns_ev_decoder, s, &ev);
			decoded = 1;
		}
	}

	if (!decoded) {
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

	ras_event_publish(ras, NON_STANDARD_EVENT, &ev);

	return 0;
}

static const struct db_fields non_standard_event_fields[] = {
		{ .name = "id",			.type = DB_TYPE_SERIAL, .is_pk = true },
		{ .name = "timestamp",		.type = DB_TYPE_TIMESTAMP, .create_index = true },
		{ .name = "sec_type",		.type = DB_TYPE_BLOB },
		{ .name = "fru_id",		.type = DB_TYPE_BLOB },
		{ .name = "fru_text",		.type = DB_TYPE_TEXT },
		{ .name = "severity",		.type = DB_TYPE_TEXT },
		{ .name = "error",		.type = DB_TYPE_BLOB },
};

const struct db_table_descriptor non_standard_event_tab = {
	.name = "non_standard_event",
	.fields = non_standard_event_fields,
	.num_fields = ARRAY_SIZE(non_standard_event_fields),
};

static struct db_desc_and_stmt non_standard_event_db = {
	.desc = &non_standard_event_tab,
};

int db_non_standard_record(struct ras_events *ras, void *priv)
{
	struct ras_non_standard_event *ev = priv;
	int rc, pos = 1;

	if (!non_standard_event_db.stmt)
		return 0;
	log(TERM, LOG_INFO, "non_standard_event store: %p\n", non_standard_event_db.stmt);

	db_bind(&non_standard_event_tab, non_standard_event_db.stmt, pos++, (uint64_t)ev->timestamp, -1);
	db_bind(&non_standard_event_tab, non_standard_event_db.stmt, pos++, (uint64_t)ev->sec_type, -1);
	db_bind(&non_standard_event_tab, non_standard_event_db.stmt, pos++, (uint64_t)ev->fru_id,  16);
	db_bind(&non_standard_event_tab, non_standard_event_db.stmt, pos++, (uint64_t)ev->fru_text, -1);
	db_bind(&non_standard_event_tab, non_standard_event_db.stmt, pos++, (uint64_t)ev->severity, -1);
	db_bind(&non_standard_event_tab, non_standard_event_db.stmt, pos++, (uint64_t)ev->error,  ev->length);

	rc = db_eval_stmt(non_standard_event_db.stmt, "non_standard_record");
	if (!rc)
		log(TERM, LOG_INFO, "register inserted at db\n");

	return rc;
}

static int ras_non_standard_db_init(struct ras_module_ctx *ctx)
{
	return ras_db_table_register(ctx, &non_standard_event_db);
}

static void ras_non_standard_db_cleanup(struct ras_module_ctx *ctx)
{
	ras_db_table_unregister(ctx);
}

static const struct ras_module_entry ras_non_standard_module = {
	.name = "non-standard-event",
	.level = BASE_EVENT_MODULE,
	.init = ras_non_standard_db_init,
	.cleanup = ras_non_standard_db_cleanup,
};

static void __attribute__((constructor)) ras_non_standard_register(void)
{
	int rc = module_register(&ras_non_standard_module);

	if (rc)
		log(TERM, LOG_ERR, "Failed to register non-standard module: %d\n", rc);
}
