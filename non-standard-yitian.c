/*
 * Copyright (C) 2023 Alibaba Inc
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "ras-record.h"
#include "ras-logger.h"
#include "ras-report.h"
#include "ras-non-standard-handler.h"
#include "non-standard-yitian.h"

static const char * const yitian_ddr_payload_err_reg_name[] = {
	"Error Type:",
	"Error SubType:",
	"Error Instance:",
	"ECCCFG0:",
	"ECCCFG1:",
	"ECCSTAT:",
	"ECCERRCNT:",
	"ECCCADDR0:",
	"ECCCADDR1:",
	"ECCCSYN0:",
	"ECCCSYN1:",
	"ECCCSYN2:",
	"ECCUADDR0:",
	"ECCUADDR1:",
	"ECCUSYN0:",
	"ECCUSYN1:",
	"ECCUSYN2:",
	"ECCBITMASK0:",
	"ECCBITMASK1:",
	"ECCBITMASK2:",
	"ADVECCSTAT:",
	"ECCAPSTAT:",
	"ECCCDATA0:",
	"ECCCDATA1:",
	"ECCUDATA0:",
	"ECCUDATA1:",
	"ECCSYMBOL:",
	"ECCERRCNTCTL:",
	"ECCERRCNTSTAT:",
	"ECCERRCNT0:",
	"ECCERRCNT1:",
	"RESERVED0:",
	"RESERVED1:",
	"RESERVED2:",
};

struct yitian_ras_type_info {
	int id;
	const char *name;
	const char * const *sub;
	int sub_num;
};

static const struct yitian_ras_type_info yitian_payload_error_type[] = {
	{
		.id = YITIAN_RAS_TYPE_DDR,
		.name = "DDR",
	},
	{
	}
};

#ifdef HAVE_SQLITE3
static const struct db_fields yitian_ddr_payload_fields[] = {
	{ .name = "id",                 .type = "INTEGER PRIMARY KEY" },
	{ .name = "timestamp",          .type = "TEXT" },
	{ .name = "address",            .type = "INTEGER" },
	{ .name = "regs_dump",		    .type = "TEXT" },
};

static const struct db_table_descriptor yitian_ddr_payload_section_tab = {
	.name = "yitian_ddr_reg_dump_event",
	.fields = yitian_ddr_payload_fields,
	.num_fields = ARRAY_SIZE(yitian_ddr_payload_fields),
};

int record_yitian_ddr_reg_dump_event(struct ras_ns_ev_decoder *ev_decoder,
				     struct ras_yitian_ddr_payload_event *ev)
{
	int rc;
	struct sqlite3_stmt *stmt = ev_decoder->stmt_dec_record;

	log(TERM, LOG_INFO, "yitian_ddr_reg_dump_event store: %p\n", stmt);

	sqlite3_bind_text(stmt,  1, ev->timestamp, -1, NULL);
	sqlite3_bind_int64(stmt,  2, ev->address);
	sqlite3_bind_text(stmt,  3, ev->reg_msg, -1, NULL);

	rc = sqlite3_step(stmt);
	if (rc != SQLITE_OK && rc != SQLITE_DONE)
		log(TERM, LOG_ERR,
		    "Failed to do yitian_ddr_reg_dump_event step on sqlite: error = %d\n", rc);
	rc = sqlite3_reset(stmt);
	if (rc != SQLITE_OK && rc != SQLITE_DONE)
		log(TERM, LOG_ERR,
		    "Failed reset yitian_ddr_reg_dump_event on sqlite: error = %d\n", rc);
	log(TERM, LOG_INFO, "register inserted at db\n");

	return rc;
}
#endif

static const char *oem_type_name(const struct yitian_ras_type_info *info,
				 uint8_t type_id)
{
	const struct yitian_ras_type_info *type = &info[0];

	for (; type->name; type++) {
		if (type->id != type_id)
			continue;
		return type->name;
	}
	return "unknown";
}

static const char *oem_subtype_name(const struct yitian_ras_type_info *info,
				    uint8_t type_id, uint8_t sub_type_id)
{
	const struct yitian_ras_type_info *type = &info[0];

	for (; type->name; type++) {
		const char * const *submodule = type->sub;

		if (type->id != type_id)
			continue;
		if (!type->sub)
			return type->name;
		if (sub_type_id >= type->sub_num)
			return "unknown";
		return submodule[sub_type_id];
	}
	return "unknown";
}

void decode_yitian_ddr_payload_err_regs(struct ras_ns_ev_decoder *ev_decoder,
					struct trace_seq *s,
				const struct yitian_ddr_payload_type_sec *err,
				struct ras_events *ras)
{
	char buf[1024];
	char *p = buf;
	char *end = buf + 1024;
	int i = 0;
	const struct yitian_payload_header *header = &err->header;
	uint32_t *pstart;
	time_t now;
	struct tm *tm;
	struct ras_yitian_ddr_payload_event ev;

	const char *type_str = oem_type_name(yitian_payload_error_type,
					    header->type);

	const char *subtype_str  = oem_subtype_name(yitian_payload_error_type,
					header->type, header->subtype);

	now = time(NULL);
	tm = localtime(&now);
	if (tm)
		strftime(ev.timestamp, sizeof(ev.timestamp),
			 "%Y-%m-%d %H:%M:%S %z", tm);
	//display error type
	p += snprintf(p, end - p, " %s", yitian_ddr_payload_err_reg_name[i++]);
	p += snprintf(p, end - p, " %s,", type_str);

	//display error subtype
	p += snprintf(p, end - p, " %s", yitian_ddr_payload_err_reg_name[i++]);
	p += snprintf(p, end - p, " %s,", subtype_str);

	//display error instance
	p += snprintf(p, end - p, " %s", yitian_ddr_payload_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%x,", header->instance);

	//display reg dump
	for (pstart = (uint32_t *)&err->ecccfg0; (void *)pstart < (void *)(err + 1); pstart += 1) {
		p += snprintf(p, end - p, " %s", yitian_ddr_payload_err_reg_name[i++]);
		p += snprintf(p, end - p, " 0x%x ", *pstart);
	}

	if (p > buf && p < end) {
		p--;
		*p = '\0';
	}

	ev.reg_msg = malloc(p - buf + 1);
	memcpy(ev.reg_msg, buf, p - buf + 1);
	ev.address = 0;

	i = 0;
	p = NULL;
	end = NULL;
	trace_seq_printf(s, "%s\n", buf);

#ifdef HAVE_SQLITE3
	record_yitian_ddr_reg_dump_event(ev_decoder, &ev);
#endif
}

static int add_yitian_common_table(struct ras_events *ras,
				   struct ras_ns_ev_decoder *ev_decoder)
{
#ifdef HAVE_SQLITE3
	if (ras->record_events && !ev_decoder->stmt_dec_record) {
		if (ras_mc_add_vendor_table(ras, &ev_decoder->stmt_dec_record,
					    &yitian_ddr_payload_section_tab) != SQLITE_OK) {
			log(TERM, LOG_WARNING,
			    "Failed to create sql yitian_ddr_payload_section_tab\n");
			return -1;
		}
	}
#endif
	return 0;
}

/* error data decoding functions */
static int decode_yitian710_ns_error(struct ras_events *ras,
				     struct ras_ns_ev_decoder *ev_decoder,
				     struct trace_seq *s,
				     struct ras_non_standard_event *event)
{
	int payload_type = event->error[0];

	if (payload_type == YITIAN_RAS_TYPE_DDR) {
		const struct yitian_ddr_payload_type_sec *err =
			(struct yitian_ddr_payload_type_sec *)event->error;
		decode_yitian_ddr_payload_err_regs(ev_decoder, s, err, ras);
	} else {
		trace_seq_printf(s, "%s: wrong payload type\n", __func__);
		return -1;
	}
	return 0;
}

struct ras_ns_ev_decoder yitian_ns_oem_decoder[] = {
	{
		.sec_type = "a6980811-16ea-4e4d-b936-fb00a23ff29c",
		.add_table = add_yitian_common_table,
		.decode = decode_yitian710_ns_error,
	},
};

static void __attribute__((constructor)) yitian_ns_init(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(yitian_ns_oem_decoder); i++)
		register_ns_ev_decoder(&yitian_ns_oem_decoder[i]);
}
