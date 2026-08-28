// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2026 Jingyu Li <joey.li@spacemit.com>
 *
 * RISC-V RAS Error Report Register Interface (RERI) handler
 */

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <traceevent/kbuffer.h>
#include <unistd.h>

#include "core/modules.h"
#include "core/ras-logger.h"
#include "core/types.h"
#include "db/ras-db.h"
#include "events-arch-riscv/ras-reri-handler.h"

static int ras_reri_event_handler(struct trace_seq *s,
				  struct tep_record *record,
				  struct tep_event *event, void *context);
static int db_reri_event(struct ras_events *ras, void *priv);

#ifdef HAVE_UNITTEST
int test_reri(void) __attribute__((weak));
#endif

static const struct ras_event_entry ras_reri_event_entry = {
	.group = "ras", .event = "reri_event",
	.handler = ras_reri_event_handler, .id = RERI_EVENT,
#ifdef HAVE_UNITTEST
	.test_group = TEST_GROUP_RISCV_EVENTS, .test = test_reri,
#endif
	.record = db_reri_event,
};
REGISTER_RAS_EVENT(ras_reri_event_entry);

#define RERI_GET_FIELD(val, offset, mask)	(((val) >> (offset)) & (mask))

#define RERI_STATUS_V_OFFSET		0
#define RERI_STATUS_V_MASK		0x1ULL
#define RERI_STATUS_CE_OFFSET		1
#define RERI_STATUS_CE_MASK		0x1ULL
#define RERI_STATUS_UED_OFFSET		2
#define RERI_STATUS_UED_MASK		0x1ULL
#define RERI_STATUS_UEC_OFFSET		3
#define RERI_STATUS_UEC_MASK		0x1ULL
#define RERI_STATUS_TT_OFFSET		8
#define RERI_STATUS_TT_MASK		0x7ULL
#define RERI_STATUS_IV_OFFSET		11
#define RERI_STATUS_IV_MASK		0x1ULL
#define RERI_STATUS_AIT_OFFSET		12
#define RERI_STATUS_AIT_MASK		0xFULL
#define RERI_STATUS_SIV_OFFSET		16
#define RERI_STATUS_SIV_MASK		0x1ULL
#define RERI_STATUS_TSV_OFFSET		17
#define RERI_STATUS_TSV_MASK		0x1ULL
#define RERI_STATUS_EC_OFFSET		24
#define RERI_STATUS_EC_MASK		0xFFULL

#define RERI_STATUS_GET_V(x)		RERI_GET_FIELD(x, RERI_STATUS_V_OFFSET, RERI_STATUS_V_MASK)
#define RERI_STATUS_GET_CE(x)		RERI_GET_FIELD(x, RERI_STATUS_CE_OFFSET, RERI_STATUS_CE_MASK)
#define RERI_STATUS_GET_UED(x)		RERI_GET_FIELD(x, RERI_STATUS_UED_OFFSET, RERI_STATUS_UED_MASK)
#define RERI_STATUS_GET_UEC(x)		RERI_GET_FIELD(x, RERI_STATUS_UEC_OFFSET, RERI_STATUS_UEC_MASK)
#define RERI_STATUS_GET_TT(x)		RERI_GET_FIELD(x, RERI_STATUS_TT_OFFSET, RERI_STATUS_TT_MASK)
#define RERI_STATUS_GET_IV(x)		RERI_GET_FIELD(x, RERI_STATUS_IV_OFFSET, RERI_STATUS_IV_MASK)
#define RERI_STATUS_GET_AIT(x)		RERI_GET_FIELD(x, RERI_STATUS_AIT_OFFSET, RERI_STATUS_AIT_MASK)
#define RERI_STATUS_GET_SIV(x)		RERI_GET_FIELD(x, RERI_STATUS_SIV_OFFSET, RERI_STATUS_SIV_MASK)
#define RERI_STATUS_GET_TSV(x)		RERI_GET_FIELD(x, RERI_STATUS_TSV_OFFSET, RERI_STATUS_TSV_MASK)
#define RERI_STATUS_GET_EC(x)		RERI_GET_FIELD(x, RERI_STATUS_EC_OFFSET, RERI_STATUS_EC_MASK)

#define CHECK_AND_ASSIGN_U8(dst, val) \
	do { \
		if ((val) > UINT8_MAX) \
			return -1; \
		(dst) = (uint8_t)(val); \
	} while (0)

#define CHECK_AND_ASSIGN_U16(dst, val) \
	do { \
		if ((val) > UINT16_MAX) \
			return -1; \
		(dst) = (uint16_t)(val); \
	} while (0)

#define CHECK_AND_ASSIGN_U32(dst, val) \
	do { \
		if ((val) > UINT32_MAX) \
			return -1; \
		(dst) = (uint32_t)(val); \
	} while (0)

#define GET_STR_FROM_ARRAY(idx, array, default_str) \
	((idx) < sizeof(array) / sizeof((array)[0]) && (array)[idx] ? \
	 (array)[idx] : (default_str))

static const char * const reri_error_code_str[] = {
	[RERI_EC_NONE] = "No error",
	[RERI_EC_OUE] = "Other unspecified error",
	[RERI_EC_CDA] = "Corrupted data access",
	[RERI_EC_CBA] = "Cache block data error",
	[RERI_EC_CSD] = "Cache scrubbing detected",
	[RERI_EC_CAS] = "Cache address/state error",
	[RERI_EC_CUE] = "Cache unspecified error",
	[RERI_EC_SDC] = "Snoop-filter/directory error",
	[RERI_EC_SUE] = "Snoop-filter/directory unspecified error",
	[RERI_EC_TPD] = "TLB/Page-walk cache data error",
	[RERI_EC_TPA] = "TLB/Page-walk address control error",
	[RERI_EC_TPU] = "TLB/Page-walk unknown error",
	[RERI_EC_HSE] = "Hart state error",
	[RERI_EC_ICS] = "Interrupt controller state error",
	[RERI_EC_ITD] = "Interconnect data error",
	[RERI_EC_ITO] = "Interconnection other error",
	[RERI_EC_IWE] = "Internal watchdog error",
	[RERI_EC_IDE] = "Internal datapath/execution unit error",
	[RERI_EC_SBE] = "System memory bus error",
	[RERI_EC_SMU] = "System memory unspecified error",
	[RERI_EC_SMD] = "System memory data error",
	[RERI_EC_SMS] = "System memory scrubbing detected",
	[RERI_EC_PIO] = "Protocol error illegal IO",
	[RERI_EC_PUS] = "Protocol error unexpected state",
	[RERI_EC_PTO] = "Protocol error timeout",
	[RERI_EC_SIC] = "System internal controller error",
	[RERI_EC_DPU] = "Deferred error passthrough not supported",
	[RERI_EC_PCX] = "PCI/CXL detected error",
};

static const char * const reri_transaction_type_str[] = {
	[RERI_TT_UNSPECIFIED] = "Unspecified",
	[RERI_TT_CUSTOM] = "Custom",
	[RERI_TT_RES1] = "Reserved",
	[RERI_TT_RES2] = "Reserved",
	[RERI_TT_EXPLICIT_READ] = "Explicit Read",
	[RERI_TT_EXPLICIT_WRITE] = "Explicit Write",
	[RERI_TT_IMPLICIT_READ] = "Implicit Read",
	[RERI_TT_IMPLICIT_WRITE] = "Implicit Write",
};

static const char * const reri_ait_str[] = {
	[RERI_AIT_NONE] = "None",
	[RERI_AIT_SPA] = "Supervisor Physical Address",
	[RERI_AIT_GPA] = "Guest Physical Address",
	[RERI_AIT_VA] = "Virtual Address",
};

static const char * const reri_source_type_str[] = {
	[RERI_SOURCE_TYPE_CPU] = "CPU",
	[RERI_SOURCE_TYPE_IOMMU] = "IOMMU",
	[RERI_SOURCE_TYPE_UNKNOWN] = "Unknown",
};

static const char * const reri_severity_str[] = {
	[RERI_SEV_INFORMATIONAL] = "Informational",
	[RERI_SEV_CORRECTED] = "Corrected",
	[RERI_SEV_RECOVERABLE] = "Recoverable",
	[RERI_SEV_FATAL] = "Fatal",
};

static const char *get_error_code_str(uint8_t ec)
{
	return GET_STR_FROM_ARRAY(ec, reri_error_code_str, "Unknown error code");
}

static const char *get_transaction_type_str(uint8_t tt)
{
	return GET_STR_FROM_ARRAY(tt, reri_transaction_type_str, "Unknown transaction type");
}

static const char *get_ait_str(uint8_t ait)
{
	return GET_STR_FROM_ARRAY(ait, reri_ait_str, "Unknown");
}

static const char *get_source_type_str(uint8_t source_type)
{
	return GET_STR_FROM_ARRAY(source_type, reri_source_type_str, "Unknown");
}

static const char *get_severity_str(uint8_t severity)
{
	return GET_STR_FROM_ARRAY(severity, reri_severity_str, "Unknown");
}

static const char *get_error_type_category(uint8_t ec)
{
	switch (ec) {
	case RERI_EC_CBA:
	case RERI_EC_CAS:
	case RERI_EC_CSD:
	case RERI_EC_CUE:
	case RERI_EC_CDA:
		return "Cache";
	case RERI_EC_TPD:
	case RERI_EC_TPA:
	case RERI_EC_TPU:
		return "TLB";
	case RERI_EC_SBE:
		return "Bus";
	case RERI_EC_HSE:
	case RERI_EC_ITD:
	case RERI_EC_ITO:
	case RERI_EC_IWE:
	case RERI_EC_IDE:
	case RERI_EC_SDC:
	case RERI_EC_SMU:
	case RERI_EC_SMD:
	case RERI_EC_SMS:
	case RERI_EC_PIO:
	case RERI_EC_PUS:
	case RERI_EC_PTO:
	case RERI_EC_SIC:
		return "Microarchitecture";
	default:
		return "Unknown";
	}
}

#ifdef HAVE_UNITTEST
const char *ras_reri_test_error_code(uint8_t value)
{
	return get_error_code_str(value);
}

const char *ras_reri_test_transaction(uint8_t value)
{
	return get_transaction_type_str(value);
}

const char *ras_reri_test_address_type(uint8_t value)
{
	return get_ait_str(value);
}

const char *ras_reri_test_category(uint8_t value)
{
	return get_error_type_category(value);
}
#endif

static int ras_reri_event_handler(struct trace_seq *s,
				  struct tep_record *record,
				  struct tep_event *event,
				  void *context)
{
	unsigned long long val;
	struct ras_events *ras = context;
	time_t now;
	struct tm *tm;
	struct ras_reri_event ev;
	uint8_t ait, tt, ec;
	uint8_t ce, ued, uec;
	const char *severity_str;

	memset(&ev, 0, sizeof(ev));

	trace_seq_printf(s, "%s ", loglevel_str[LOGLEVEL_ERR]);

	if (ras->use_uptime)
		now = record->ts / user_hz + ras->uptime_diff;
	else
		now = time(NULL);
#ifdef HAVE_CPU_FAULT_ISOLATION
	ev.event_time = now;
#endif

	tm = localtime(&now);
	if (tm)
		strftime(ev.timestamp, sizeof(ev.timestamp),
			 "%Y-%m-%d %H:%M:%S %z", tm);
	trace_seq_printf(s, "%s", ev.timestamp);

	if (tep_get_field_val(s, event, "err_src_id", record, &val, 1) < 0)
		return -1;
	CHECK_AND_ASSIGN_U16(ev.err_src_id, val);
	trace_seq_printf(s, " err_src_id: 0x%x", ev.err_src_id);

	if (tep_get_field_val(s, event, "source_type", record, &val, 1) < 0)
		return -1;
	CHECK_AND_ASSIGN_U8(ev.source_type, val);
	trace_seq_printf(s, " source: %s", get_source_type_str(ev.source_type));

	if (ev.source_type == RERI_SOURCE_TYPE_CPU) {
		if (tep_get_field_val(s, event, "hart_id", record, &val, 1) >= 0) {
			CHECK_AND_ASSIGN_U32(ev.hart_id, val);
#ifdef HAVE_CPU_FAULT_ISOLATION
			ev.hart_id_valid = true;
#endif
			trace_seq_printf(s, " hart_id: %u", ev.hart_id);
		}
		if (tep_get_field_val(s, event, "cluster_id", record, &val, 1) >= 0) {
			CHECK_AND_ASSIGN_U32(ev.cluster_id, val);
			trace_seq_printf(s, " cluster_id: %u", ev.cluster_id);
		}
	}

	if (tep_get_field_val(s, event, "status", record, &val, 1) < 0)
		return -1;
	ev.status = val;

	ait = RERI_STATUS_GET_AIT(ev.status);
	tt = RERI_STATUS_GET_TT(ev.status);
	ec = RERI_STATUS_GET_EC(ev.status);
	ce = RERI_STATUS_GET_CE(ev.status);
	ued = RERI_STATUS_GET_UED(ev.status);
	uec = RERI_STATUS_GET_UEC(ev.status);

	if (uec)
		ev.severity = RERI_SEV_FATAL;
	else if (ued)
		ev.severity = RERI_SEV_RECOVERABLE;
	else if (ce)
		ev.severity = RERI_SEV_CORRECTED;
	else
		ev.severity = RERI_SEV_INFORMATIONAL;

	severity_str = get_severity_str(ev.severity);
	trace_seq_printf(s, "\n severity: %s", severity_str);

	trace_seq_printf(s, "\n error_type: %s", get_error_type_category(ec));
	trace_seq_printf(s, "\n error_code: 0x%02x (%s)", ec, get_error_code_str(ec));

	if (tt != 0)
		trace_seq_printf(s, "\n transaction_type: 0x%02x (%s)",
				 tt, get_transaction_type_str(tt));

	if (ait != RERI_AIT_NONE) {
		if (tep_get_field_val(s, event, "addr_info", record, &val, 1) >= 0) {
			ev.addr_info = val;
			trace_seq_printf(s, "\n address_type: %s", get_ait_str(ait));
			trace_seq_printf(s, "\n address: 0x%llx",
					 (unsigned long long)ev.addr_info);
		}
	}

	if (RERI_STATUS_GET_IV(ev.status)) {
		if (tep_get_field_val(s, event, "info", record, &val, 1) >= 0) {
			ev.info = val;
			trace_seq_printf(s, "\n info: 0x%llx",
					 (unsigned long long)ev.info);
		}
	}

	if (RERI_STATUS_GET_SIV(ev.status)) {
		if (tep_get_field_val(s, event, "suppl_info", record, &val, 1) >= 0) {
			ev.suppl_info = val;
			trace_seq_printf(s, "\n suppl_info: 0x%llx",
					 (unsigned long long)ev.suppl_info);
		}
	}

	if (RERI_STATUS_GET_TSV(ev.status)) {
		if (tep_get_field_val(s, event, "timestamp_val", record, &val, 1) >= 0) {
			ev.timestamp_val = val;
			trace_seq_printf(s, "\n hw_timestamp: 0x%llx",
					 (unsigned long long)ev.timestamp_val);
		}
	}

	trace_seq_printf(s, "\n status_reg: 0x%llx",
			 (unsigned long long)ev.status);

	trace_seq_puts(s, "\n");

	ras_event_publish(ras, RERI_EVENT, &ev);

	return 0;
}
static const struct db_fields reri_event_fields[] = {
	{ .name = "id",			.type = DB_TYPE_SERIAL, .is_pk = true },
	{ .name = "timestamp",		.type = DB_TYPE_TIMESTAMP, .create_index = true },
	{ .name = "err_src_id",		.type = DB_TYPE_INT32 },
	{ .name = "source_type",	.type = DB_TYPE_INT32 },
	{ .name = "severity",		.type = DB_TYPE_INT32 },
	{ .name = "hart_id",		.type = DB_TYPE_INT32 },
	{ .name = "cluster_id",		.type = DB_TYPE_INT32 },
	{ .name = "status",		.type = DB_TYPE_INT64 },
	{ .name = "addr_info",		.type = DB_TYPE_INT64 },
	{ .name = "info",		.type = DB_TYPE_INT64 },
	{ .name = "suppl_info",		.type = DB_TYPE_INT64 },
	{ .name = "timestamp_val",	.type = DB_TYPE_INT64 },
};

static const struct db_table_descriptor reri_event_tab = {
	.name = "reri_event",
	.fields = reri_event_fields,
	.num_fields = ARRAY_SIZE(reri_event_fields),
};

static struct db_desc_and_stmt reri_event_db = {
	.desc = &reri_event_tab,
};

static int db_reri_event(struct ras_events *ras, void *priv)
{
	struct ras_reri_event *ev = priv;
	int rc, pos = 1;

	if (!reri_event_db.stmt)
		return 0;
	log(TERM, LOG_INFO, "reri_event store: %p\n", reri_event_db.stmt);

	db_bind(&reri_event_tab, reri_event_db.stmt, pos++, (uint64_t)ev->timestamp, -1);
	db_bind(&reri_event_tab, reri_event_db.stmt, pos++, ev->err_src_id, -1);
	db_bind(&reri_event_tab, reri_event_db.stmt, pos++, ev->source_type, -1);
	db_bind(&reri_event_tab, reri_event_db.stmt, pos++, ev->severity, -1);
	db_bind(&reri_event_tab, reri_event_db.stmt, pos++, ev->hart_id, -1);
	db_bind(&reri_event_tab, reri_event_db.stmt, pos++, ev->cluster_id, -1);
	db_bind(&reri_event_tab, reri_event_db.stmt, pos++, ev->status, -1);
	db_bind(&reri_event_tab, reri_event_db.stmt, pos++, ev->addr_info, -1);
	db_bind(&reri_event_tab, reri_event_db.stmt, pos++, ev->info, -1);
	db_bind(&reri_event_tab, reri_event_db.stmt, pos++, ev->suppl_info, -1);
	db_bind(&reri_event_tab, reri_event_db.stmt, pos++, ev->timestamp_val, -1);

	rc = db_eval_stmt(reri_event_db.stmt, "reri_event");
	if (!rc)
		log(TERM, LOG_INFO, "register inserted at db\n");

	return rc;
}

static int ras_reri_db_init(struct ras_module_ctx *ctx)
{
	return ras_db_table_register(ctx, &reri_event_db);
}

static void ras_reri_db_cleanup(struct ras_module_ctx *ctx)
{
	ras_db_table_unregister(ctx);
}

static const struct ras_module_entry ras_reri_module = {
	.name = "reri-event",
	.level = BASE_EVENT_MODULE,
	.init = ras_reri_db_init,
	.cleanup = ras_reri_db_cleanup,
};

REGISTER_RAS_MODULE(ras_reri_module);
