// SPDX-License-Identifier: GPL-2.0-only

/*
 * Copyright (C) 2014 Tony Luck <tony.luck@intel.com>
 */

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <traceevent/kbuffer.h>
#include <unistd.h>

#include "core/modules.h"
#include "core/ras-logger.h"
#include "core/types.h"
#include "events/ras-extlog-handler.h"

static int ras_extlog_mem_event_handler(struct trace_seq *s,
					struct tep_record *record,
					struct tep_event *event, void *context);
int db_extlog_mem_record(struct ras_events *ras, void *priv);

#ifdef HAVE_UNITTEST
int test_extlog(void) __attribute__((weak));
#endif

static void ras_extlog_enabled(struct ras_events *ras)
{
	ras->daemon_active_fd =
		open("/sys/kernel/debug/ras/daemon_active", O_RDONLY);
	if (ras->daemon_active_fd < 0)
		log(TERM, LOG_WARNING, "Can't mark extlog daemon active\n");
}

static const struct ras_event_entry ras_extlog_event = {
	.group = "ras", .event = "extlog_mem_event",
	.handler = ras_extlog_mem_event_handler, .id = EXTLOG_EVENT,
	.trigger = true, .enabled = ras_extlog_enabled,
#ifdef HAVE_UNITTEST
	.test_group = TEST_GROUP_EVENTS, .test = test_extlog,
#endif
	.record = db_extlog_mem_record,
};
REGISTER_RAS_EVENT(ras_extlog_event);
#include "modules/ras-report.h"

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

#ifdef HAVE_UNITTEST
const char *ras_extlog_test_error_type(int type)
{
	return err_type(type);
}

const char *ras_extlog_test_severity(int severity)
{
	return err_severity(severity);
}

unsigned long long ras_extlog_test_mask(int lsb)
{
	return err_mask(lsb);
}
#endif

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
	unsigned int rc, size = sizeof(buf);
	char *p = buf;

	if (cpd->validation_bits == 0)
		return "";
	rc = snprintf(p, size, " (");
	p += rc;
	size -= rc;
	if (cpd->validation_bits & CPER_MEM_VALID_NODE) {
		rc = snprintf(p, size, "node: %d ", cpd->node);
		p += rc;
		size -= rc;
	}
	if (cpd->validation_bits & CPER_MEM_VALID_CARD) {
		rc = snprintf(p, size, "card: %d ", cpd->card);
		p += rc;
		size -= rc;
	}
	if (cpd->validation_bits & CPER_MEM_VALID_MODULE) {
		rc = snprintf(p, size, "module: %d ", cpd->module);
		p += rc;
		size -= rc;
	}
	if (cpd->validation_bits & CPER_MEM_VALID_BANK) {
		rc = snprintf(p, size, "bank: %d ", cpd->bank);
		p += rc;
		size -= rc;
	}
	if (cpd->validation_bits & CPER_MEM_VALID_DEVICE) {
		rc = snprintf(p, size, "device: %d ", cpd->device);
		p += rc;
		size -= rc;
	}
	if (cpd->validation_bits & CPER_MEM_VALID_ROW) {
		rc = snprintf(p, size, "row: %d ", cpd->row);
		p += rc;
		size -= rc;
	}
	if (cpd->validation_bits & CPER_MEM_VALID_COLUMN) {
		rc = snprintf(p, size, "column: %d ", cpd->column);
		p += rc;
		size -= rc;
	}
	if (cpd->validation_bits & CPER_MEM_VALID_BIT_POSITION) {
		rc = snprintf(p, size, "bit_pos: %d ", cpd->bit_pos);
		p += rc;
		size -= rc;
	}
	if (cpd->validation_bits & CPER_MEM_VALID_REQUESTOR_ID) {
		rc = snprintf(p, size, "req_id: 0x%llx ", cpd->requestor_id);
		p += rc;
		size -= rc;
	}
	if (cpd->validation_bits & CPER_MEM_VALID_RESPONDER_ID) {
		rc = snprintf(p, size, "resp_id: 0x%llx ", cpd->responder_id);
		p += rc;
		size -= rc;
	}
	if (cpd->validation_bits & CPER_MEM_VALID_TARGET_ID) {
		rc = snprintf(p, size, "tgt_id: 0x%llx ", cpd->target_id);
		p += rc;
		size -= rc;
	}
	if (cpd->validation_bits & CPER_MEM_VALID_RANK_NUMBER) {
		rc = snprintf(p, size, "rank: %d ", cpd->rank);
		p += rc;
		size -= rc;
	}
	if (cpd->validation_bits & CPER_MEM_VALID_CARD_HANDLE) {
		rc = snprintf(p, size, "card_handle: %d ", cpd->mem_array_handle);
		p += rc;
		size -= rc;
	}
	if (cpd->validation_bits & CPER_MEM_VALID_MODULE_HANDLE) {
		rc = snprintf(p, size, "module_handle: %d ", cpd->mem_dev_handle);
		p += rc;
		size -= rc;
	}
	rc = snprintf(p - 1, size, ")");

	return buf;
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

static void report_extlog_mem_event(struct ras_events *ras,
				    struct tep_record *record,
				    struct trace_seq *s,
				    struct ras_extlog_event *ev)
{
	const char *level;

	switch (ev->severity) {
	case 0:
		level = loglevel_str[LOGLEVEL_CRIT];
		break;
	case 1:
		level = loglevel_str[LOGLEVEL_EMERG];
		break;
	case 2:
		level = loglevel_str[LOGLEVEL_ERR];
		break;
	case 3:
		level = loglevel_str[LOGLEVEL_INFO];
		break;
	default:
		level = loglevel_str[LOGLEVEL_DEBUG];
		break;
	}
	trace_seq_printf(s, "%s ", level);
	trace_seq_printf(s, "%d %s error: %s physical addr: 0x%llx mask: 0x%llx%s %s %s",
			 ev->error_seq, err_severity(ev->severity),
		err_type(ev->etype), ev->address,
		err_mask(ev->pa_mask_lsb),
		err_cper_data(ev->cper_data),
		ev->fru_text,
		uuid_le(ev->fru_id));
}

static int ras_extlog_mem_event_handler(struct trace_seq *s,
				 struct tep_record *record,
				 struct tep_event *event, void *context)
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
		now = record->ts / user_hz + ras->uptime_diff;
	else
		now = time(NULL);

	tm = localtime(&now);
	if (tm)
		strftime(ev.timestamp, sizeof(ev.timestamp),
			 "%Y-%m-%d %H:%M:%S %z", tm);
	trace_seq_printf(s, "%s ", ev.timestamp);

	if (tep_get_field_val(s,  event, "etype", record, &val, 1) < 0)
		return -1;
	ev.etype = val;
	if (tep_get_field_val(s,  event, "err_seq", record, &val, 1) < 0)
		return -1;
	ev.error_seq = val;
	if (tep_get_field_val(s,  event, "sev", record, &val, 1) < 0)
		return -1;
	ev.severity = val;
	if (tep_get_field_val(s,  event, "pa", record, &val, 1) < 0)
		return -1;
	ev.address = val;
	if (tep_get_field_val(s,  event, "pa_mask_lsb", record, &val, 1) < 0)
		return -1;
	ev.pa_mask_lsb = val;

	ev.cper_data = tep_get_field_raw(s, event, "data",
					 record, &len, 1);
	ev.cper_data_length = len;
	ev.fru_text = tep_get_field_raw(s, event, "fru_text",
					record, &len, 1);
	ev.fru_id = tep_get_field_raw(s, event, "fru_id",
				      record, &len, 1);

	report_extlog_mem_event(ras, record, s, &ev);

	db_extlog_mem_record(ras, &ev);

	return 0;
}
static const struct db_fields extlog_event_fields[] = {
	{ .name = "id",			.type = DB_TYPE_SERIAL, .is_pk = true },
	{ .name = "timestamp",		.type = DB_TYPE_TIMESTAMP, .create_index = true },
	{ .name = "etype",		.type = DB_TYPE_INT32 },
	{ .name = "error_count",	.type = DB_TYPE_INT32 },
	{ .name = "severity",		.type = DB_TYPE_INT32 },
	{ .name = "address",		.type = DB_TYPE_INT64 },
	{ .name = "fru_id",		.type = DB_TYPE_BLOB },
	{ .name = "fru_text",		.type = DB_TYPE_TEXT },
	{ .name = "cper_data",		.type = DB_TYPE_BLOB },
};

const struct db_table_descriptor extlog_event_tab = {
	.name = "extlog_event",
	.fields = extlog_event_fields,
	.num_fields = ARRAY_SIZE(extlog_event_fields),
};

static struct db_desc_and_stmt extlog_event_db = {
	.desc = &extlog_event_tab,
};

int db_extlog_mem_record(struct ras_events *ras, void *priv)
{
	struct ras_extlog_event *ev = priv;
	int rc, pos = 1;

	if (!extlog_event_db.stmt)
		return 0;
	log(TERM, LOG_INFO, "extlog_record store: %p\n", extlog_event_db.stmt);

	db_bind(&extlog_event_tab, extlog_event_db.stmt, pos++, (uint64_t)ev->timestamp, -1);
	db_bind(&extlog_event_tab, extlog_event_db.stmt, pos++, ev->etype, -1);
	db_bind(&extlog_event_tab, extlog_event_db.stmt, pos++, ev->error_seq, -1);
	db_bind(&extlog_event_tab, extlog_event_db.stmt, pos++, ev->severity, -1);
	db_bind(&extlog_event_tab, extlog_event_db.stmt, pos++, ev->address, -1);
	db_bind(&extlog_event_tab, extlog_event_db.stmt, pos++, (uint64_t)ev->fru_id,  16);
	db_bind(&extlog_event_tab, extlog_event_db.stmt, pos++, (uint64_t)ev->fru_text, -1);
	db_bind(&extlog_event_tab, extlog_event_db.stmt, pos++, (uint64_t)ev->cper_data,  ev->cper_data_length);

	rc = db_eval_stmt(extlog_event_db.stmt, "extlog_record");
	if (!rc)
		log(TERM, LOG_INFO, "register inserted at db\n");

	return rc;
}

static int ras_extlog_db_init(struct ras_module_ctx *ctx)
{
	return ras_db_table_register(ctx, &extlog_event_db);
}

static void ras_extlog_db_cleanup(struct ras_module_ctx *ctx)
{
	ras_db_table_unregister(ctx);
}

static const struct ras_module_entry ras_extlog_module = {
	.name = "extlog-event",
	.level = BASE_EVENT_MODULE,
	.init = ras_extlog_db_init,
	.cleanup = ras_extlog_db_cleanup,
};

static void __attribute__((constructor)) ras_extlog_register(void)
{
	int rc = module_register(&ras_extlog_module);

	if (rc)
		log(TERM, LOG_ERR, "Failed to register EXTLOG module: %d\n", rc);
}
