// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * Copyright (C) 2023 Alibaba Inc
 */

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/ras-logger.h"
#include "core/modules.h"
#include "core/types.h"
#include "events-arch-arm/non-standard-yitian.h"
#include "events-arch-arm/ras-non-standard-handler.h"
#include "modules/ras-report.h"

static bool append_text(char **cursor, char *end, const char *fmt, ...)
{
	va_list ap;
	int n;
	size_t remaining;

	if (*cursor >= end)
		return false;
	remaining = end - *cursor;
	va_start(ap, fmt);
	n = vsnprintf(*cursor, remaining, fmt, ap);
	va_end(ap);
	if (n < 0)
		return false;
	if ((size_t)n >= remaining) {
		*cursor = end - 1;
		return false;
	}
	*cursor += n;
	return true;
}

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

static const struct db_fields yitian_ddr_payload_fields[] = {
	{ .name = "id",			.type = DB_TYPE_SERIAL, .is_pk = true },
	{ .name = "timestamp",		.type = DB_TYPE_TIMESTAMP},
	{ .name = "address",		.type = DB_TYPE_INT64 },
	{ .name = "regs_dump",		.type = DB_TYPE_TEXT },
};

static const struct db_table_descriptor yitian_ddr_payload_section_tab = {
	.name = "yitian_ddr_reg_dump_event",
	.fields = yitian_ddr_payload_fields,
	.num_fields = ARRAY_SIZE(yitian_ddr_payload_fields),
};

static struct db_desc_and_stmt yitian_ddr_payload_section_db = {
	.desc = &yitian_ddr_payload_section_tab,
};

static int record_yitian_ddr_reg_dump_event(struct ras_yitian_ddr_payload_event *ev)
{
	struct ras_stmt *stmt = yitian_ddr_payload_section_db.stmt;
	int pos = 1;

	log(TERM, LOG_INFO, "yitian_ddr_reg_dump_event store: %p\n",
	    stmt);

	db_bind(&yitian_ddr_payload_section_tab, stmt, pos++,
		(uint64_t)ev->timestamp, -1);
	db_bind(&yitian_ddr_payload_section_tab, stmt, pos++, ev->address, -1);
	db_bind(&yitian_ddr_payload_section_tab, stmt, pos++,
		(uint64_t)ev->reg_msg, -1);

	return db_eval_stmt(stmt, yitian_ddr_payload_section_db.desc->name);
}

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

static void decode_yitian_ddr_err_regs(struct trace_seq *s,
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
	struct tm tm;
	struct ras_yitian_ddr_payload_event ev = { 0 };

	const char *type_str = oem_type_name(yitian_payload_error_type,
					    header->type);

	const char *subtype_str  = oem_subtype_name(yitian_payload_error_type,
					header->type, header->subtype);

	now = time(NULL);
	if (localtime_r(&now, &tm))
		strftime(ev.timestamp, sizeof(ev.timestamp),
			 "%Y-%m-%d %H:%M:%S %z", &tm);
	//display error type
	append_text(&p, end, " %s", yitian_ddr_payload_err_reg_name[i++]);
	append_text(&p, end, " %s,", type_str);

	//display error subtype
	append_text(&p, end, " %s", yitian_ddr_payload_err_reg_name[i++]);
	append_text(&p, end, " %s,", subtype_str);

	//display error instance
	append_text(&p, end, " %s", yitian_ddr_payload_err_reg_name[i++]);
	append_text(&p, end, " 0x%x,", header->instance);

	//display reg dump
	for (pstart = (uint32_t *)&err->ecccfg0; (void *)pstart < (void *)(err + 1); pstart += 1) {
		if (!append_text(&p, end, " %s", yitian_ddr_payload_err_reg_name[i++]) ||
		    !append_text(&p, end, " 0x%x ", *pstart))
			break;
	}

	if (p > buf && p < end) {
		p--;
		*p = '\0';
	}

	ev.reg_msg = buf;
	ev.address = 0;

	i = 0;
	p = NULL;
	end = NULL;
	trace_seq_printf(s, "%s\n", buf);

	WARN_ONCE(ras->record_events && !yitian_ddr_payload_section_db.stmt,
		  ALL, LOG_WARNING, "Can't insert into table %s: no statement\n",
		  yitian_ddr_payload_section_db.desc->name);
	record_yitian_ddr_reg_dump_event(&ev);
}

/* error data decoding functions */
static int decode_yitian710_ns_error(struct ras_events *ras,
				     struct ras_ns_ev_decoder *ev_decoder,
				     struct trace_seq *s,
				     struct ras_non_standard_event *event)
{
	if (event->length < (int)sizeof(struct yitian_payload_header)) {
		trace_seq_printf(s, "%s: truncated payload\n", __func__);
		return -1;
	}
	int payload_type = event->error[0];

	if (payload_type == YITIAN_RAS_TYPE_DDR) {
		if (event->length < (int)sizeof(struct yitian_ddr_payload_type_sec)) {
			trace_seq_printf(s, "%s: truncated DDR payload\n", __func__);
			return -1;
		}
		const struct yitian_ddr_payload_type_sec *err =
			(struct yitian_ddr_payload_type_sec *)event->error;
		decode_yitian_ddr_err_regs(s, err, ras);
	} else {
		trace_seq_printf(s, "%s: wrong payload type\n", __func__);
		return -1;
	}
	return 0;
}

struct ras_ns_ev_decoder yitian_ns_oem_decoder[] = {
	{
		.sec_type = "a6980811-16ea-4e4d-b936-fb00a23ff29c",
		.decode = decode_yitian710_ns_error,
	},
};

static int yitian_ns_init(struct ras_module_ctx *ctx)
{
	int i;
	int rc;

	rc = ras_db_table_register(ctx, &yitian_ddr_payload_section_db);
	if (rc)
		return rc;

	for (i = 0; i < ARRAY_SIZE(yitian_ns_oem_decoder); i++) {
		rc = register_ns_ev_decoder(&yitian_ns_oem_decoder[i]);
		if (rc) {
			ras_db_table_unregister(ctx);
			return rc;
		}
	}

	return 0;
}

static void yitian_ns_cleanup(struct ras_module_ctx *ctx)
{
	ras_db_table_unregister(ctx);
}

static const struct ras_module_entry yitian_ns_module = {
	.name = "non-standard-yitian",
	.level = SUB_EVENT_MODULE,
	.init = yitian_ns_init,
	.cleanup = yitian_ns_cleanup,
};

static void __attribute__((constructor)) yitian_ns_register(void)
{
	int rc = module_register(&yitian_ns_module);

	if (rc)
		log(TERM, LOG_ERR, "Failed to register Yitian module: %d\n", rc);
}

#ifdef HAVE_UNITTEST
struct db_table_descriptor_list yitian_table_descriptors(void)
{
	static const struct db_table_descriptor * const tables[] = {
		&yitian_ddr_payload_section_tab,
	};

	return (struct db_table_descriptor_list) { tables, ARRAY_SIZE(tables) };
}
#endif
