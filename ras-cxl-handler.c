// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 */

#include <endian.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <traceevent/kbuffer.h>
#include <unistd.h>

#include "ras-cxl-handler.h"
#include "ras-page-isolation.h"
#include "ras-logger.h"
#include "ras-record.h"
#include "ras-report.h"
#include "types.h"

/* Common Functions */
static void convert_timestamp(unsigned long long ts, char *ts_ptr, uint16_t size)
{
	/* CXL Specification 3.0
	 * Overflow timestamp - The number of unsigned nanoseconds
	 * that have elapsed since midnight, 01-Jan-1970 UTC
	 */
	time_t ts_secs = ts / 1000000000ULL;
	struct tm *tm;

	tm = localtime(&ts_secs);
	if (tm)
		strftime(ts_ptr, size, "%Y-%m-%d %H:%M:%S %z", tm);

	if (!ts || !tm)
		strscpy(ts_ptr, "1970-01-01 00:00:00 +0000",
			size);
}

static void get_timestamp(struct trace_seq *s, struct tep_record *record,
			  struct ras_events *ras, char *ts_ptr, uint16_t size)
{
	time_t now;
	struct tm *tm;

	now = record->ts / user_hz + ras->uptime_diff;
	tm = localtime(&now);
	if (tm)
		strftime(ts_ptr, size, "%Y-%m-%d %H:%M:%S %z", tm);
	else
		strscpy(ts_ptr, "1970-01-01 00:00:00 +0000", size);
}

struct cxl_event_flags {
	uint32_t bit;
	const char *flag;
};

static int decode_cxl_event_flags(struct trace_seq *s, uint32_t flags,
				  const struct cxl_event_flags *cxl_ev_flags,
				  uint8_t num_elems)
{
	int i;

	for (i = 0; i < num_elems; i++) {
		if (flags & cxl_ev_flags[i].bit)
			if (trace_seq_printf(s, "\'%s\' ", cxl_ev_flags[i].flag) <= 0)
				return -1;
	}
	return 0;
}

static char *uuid_be(const char *uu)
{
	static char uuid[sizeof("xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx")];
	char *p = uuid;
	int i;
	static const unsigned char be[16] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};

	for (i = 0; i < 16; i++) {
		p += snprintf(p, sizeof(uuid), "%.2x", (unsigned char)uu[be[i]]);
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

static const char * const get_cxl_type_str(const char * const *type_array,
					   uint8_t num_elems, uint8_t type)
{
	if (type >= num_elems)
		return "Unknown";

	return type_array[type];
}

/* Poison List: Payload out flags */
#define CXL_POISON_FLAG_MORE            BIT(0)
#define CXL_POISON_FLAG_OVERFLOW        BIT(1)
#define CXL_POISON_FLAG_SCANNING        BIT(2)

/* CXL poison - source types */
enum cxl_poison_source {
	CXL_POISON_SOURCE_UNKNOWN = 0,
	CXL_POISON_SOURCE_EXTERNAL = 1,
	CXL_POISON_SOURCE_INTERNAL = 2,
	CXL_POISON_SOURCE_INJECTED = 3,
	CXL_POISON_SOURCE_VENDOR = 7,
};

/* CXL poison - trace types */
enum cxl_poison_trace_type {
	CXL_POISON_TRACE_LIST,
	CXL_POISON_TRACE_INJECT,
	CXL_POISON_TRACE_CLEAR,
};

int ras_cxl_poison_event_handler(struct trace_seq *s,
				 struct tep_record *record,
				 struct tep_event *event, void *context)
{
	int len;
	unsigned long long val;
	struct ras_events *ras = context;
	struct ras_cxl_poison_event ev;

	get_timestamp(s, record, ras, (char *)&ev.timestamp, sizeof(ev.timestamp));
	if (trace_seq_printf(s, "%s ", ev.timestamp) <= 0)
		return -1;

	ev.memdev = tep_get_field_raw(s, event, "memdev",
				      record, &len, 1);
	if (!ev.memdev)
		return -1;
	if (trace_seq_printf(s, "memdev:%s ", ev.memdev) <= 0)
		return -1;

	ev.host = tep_get_field_raw(s, event, "host",
				    record, &len, 1);
	if (!ev.host)
		return -1;
	if (trace_seq_printf(s, "host:%s ", ev.host) <= 0)
		return -1;

	if (tep_get_field_val(s, event, "serial", record, &val, 1) < 0)
		return -1;
	ev.serial = val;
	if (trace_seq_printf(s, "serial:0x%llx ", (unsigned long long)ev.serial) <= 0)
		return -1;

	if (tep_get_field_val(s,  event, "trace_type", record, &val, 1) < 0)
		return -1;
	switch (val) {
	case CXL_POISON_TRACE_LIST:
		ev.trace_type = "List";
		break;
	case CXL_POISON_TRACE_INJECT:
		ev.trace_type = "Inject";
		break;
	case CXL_POISON_TRACE_CLEAR:
		ev.trace_type = "Clear";
		break;
	default:
		ev.trace_type = "Invalid";
	}
	if (trace_seq_printf(s, "trace_type:%s ", ev.trace_type) <= 0)
		return -1;

	ev.region = tep_get_field_raw(s, event, "region",
				      record, &len, 1);
	if (!ev.region)
		return -1;
	if (trace_seq_printf(s, "region:%s ", ev.region) <= 0)
		return -1;

	ev.uuid = tep_get_field_raw(s, event, "uuid",
				    record, &len, 1);
	if (!ev.uuid)
		return -1;
	if (trace_seq_printf(s, "region_uuid:%s ", ev.uuid) <= 0)
		return -1;

	if (tep_get_field_val(s, event, "hpa", record, &val, 1) < 0)
		return -1;
	ev.hpa = val;
	if (trace_seq_printf(s, "poison list: hpa:0x%llx ", (unsigned long long)ev.hpa) <= 0)
		return -1;

	if (tep_get_field_val(s, event, "dpa", record, &val, 1) < 0)
		return -1;
	ev.dpa = val;
	if (trace_seq_printf(s, "dpa:0x%llx ", (unsigned long long)ev.dpa) <= 0)
		return -1;

	if (tep_get_field_val(s, event, "dpa_length", record, &val, 1) < 0)
		return -1;
	ev.dpa_length = val;
	if (trace_seq_printf(s, "dpa_length:0x%x ", ev.dpa_length) <= 0)
		return -1;

	if (tep_get_field_val(s,  event, "source", record, &val, 1) < 0)
		return -1;
	switch (val) {
	case CXL_POISON_SOURCE_UNKNOWN:
		ev.source = "Unknown";
		break;
	case CXL_POISON_SOURCE_EXTERNAL:
		ev.source = "External";
		break;
	case CXL_POISON_SOURCE_INTERNAL:
		ev.source = "Internal";
		break;
	case CXL_POISON_SOURCE_INJECTED:
		ev.source = "Injected";
		break;
	case CXL_POISON_SOURCE_VENDOR:
		ev.source = "Vendor";
		break;
	default:
		ev.source = "Invalid";
	}
	if (trace_seq_printf(s, "source:%s ", ev.source) <= 0)
		return -1;

	if (tep_get_field_val(s,  event, "flags", record, &val, 1) < 0)
		return -1;
	ev.flags = val;
	if (trace_seq_printf(s, "flags:%d ", ev.flags) <= 0)
		return -1;

	if (ev.flags & CXL_POISON_FLAG_OVERFLOW) {
		if (tep_get_field_val(s,  event, "overflow_ts", record, &val, 1) < 0)
			return -1;
		convert_timestamp(val, ev.overflow_ts, sizeof(ev.overflow_ts));
	} else {
		strscpy(ev.overflow_ts, "1970-01-01 00:00:00 +0000",
			sizeof(ev.overflow_ts));
	}

	if (trace_seq_printf(s, "overflow timestamp:%s\n", ev.overflow_ts) <= 0)
		return -1;

	/* Insert data into the SGBD */
#ifdef HAVE_SQLITE3
	ras_store_cxl_poison_event(ras, &ev);
#endif

#ifdef HAVE_ABRT_REPORT
	/* Report event to ABRT */
	ras_report_cxl_poison_event(ras, &ev);
#endif

	return 0;
}

/* CXL AER Errors */

#define CXL_AER_UE_CACHE_DATA_PARITY	BIT(0)
#define CXL_AER_UE_CACHE_ADDR_PARITY	BIT(1)
#define CXL_AER_UE_CACHE_BE_PARITY	BIT(2)
#define CXL_AER_UE_CACHE_DATA_ECC	BIT(3)
#define CXL_AER_UE_MEM_DATA_PARITY	BIT(4)
#define CXL_AER_UE_MEM_ADDR_PARITY	BIT(5)
#define CXL_AER_UE_MEM_BE_PARITY	BIT(6)
#define CXL_AER_UE_MEM_DATA_ECC		BIT(7)
#define CXL_AER_UE_REINIT_THRESH	BIT(8)
#define CXL_AER_UE_RSVD_ENCODE		BIT(9)
#define CXL_AER_UE_POISON		BIT(10)
#define CXL_AER_UE_RECV_OVERFLOW	BIT(11)
#define CXL_AER_UE_INTERNAL_ERR		BIT(14)
#define CXL_AER_UE_IDE_TX_ERR		BIT(15)
#define CXL_AER_UE_IDE_RX_ERR		BIT(16)

#define CXL_AER_CE_CACHE_DATA_ECC	BIT(0)
#define CXL_AER_CE_MEM_DATA_ECC		BIT(1)
#define CXL_AER_CE_CRC_THRESH		BIT(2)
#define CXL_AER_CE_RETRY_THRESH		BIT(3)
#define CXL_AER_CE_CACHE_POISON		BIT(4)
#define CXL_AER_CE_MEM_POISON		BIT(5)
#define CXL_AER_CE_PHYS_LAYER_ERR	BIT(6)

struct cxl_error_list {
	uint32_t bit;
	const char *error;
};

static const struct cxl_error_list cxl_aer_ue[] = {
	{ .bit = CXL_AER_UE_CACHE_DATA_PARITY, .error = "Cache Data Parity Error" },
	{ .bit = CXL_AER_UE_CACHE_ADDR_PARITY, .error = "Cache Address Parity Error" },
	{ .bit = CXL_AER_UE_CACHE_BE_PARITY, .error = "Cache Byte Enable Parity Error" },
	{ .bit = CXL_AER_UE_CACHE_DATA_ECC, .error = "Cache Data ECC Error" },
	{ .bit = CXL_AER_UE_MEM_DATA_PARITY, .error = "Memory Data Parity Error" },
	{ .bit = CXL_AER_UE_MEM_ADDR_PARITY, .error = "Memory Address Parity Error" },
	{ .bit = CXL_AER_UE_MEM_BE_PARITY, .error = "Memory Byte Enable Parity Error" },
	{ .bit = CXL_AER_UE_MEM_DATA_ECC, .error = "Memory Data ECC Error" },
	{ .bit = CXL_AER_UE_REINIT_THRESH, .error = "REINIT Threshold Hit" },
	{ .bit = CXL_AER_UE_RSVD_ENCODE, .error = "Received Unrecognized Encoding" },
	{ .bit = CXL_AER_UE_POISON, .error = "Received Poison From Peer" },
	{ .bit = CXL_AER_UE_RECV_OVERFLOW, .error = "Receiver Overflow" },
	{ .bit = CXL_AER_UE_INTERNAL_ERR, .error = "Component Specific Error" },
	{ .bit = CXL_AER_UE_IDE_TX_ERR, .error = "IDE Tx Error" },
	{ .bit = CXL_AER_UE_IDE_RX_ERR, .error = "IDE Rx Error" },
};

static const struct cxl_error_list cxl_aer_ce[] = {
	{ .bit = CXL_AER_CE_CACHE_DATA_ECC, .error = "Cache Data ECC Error" },
	{ .bit = CXL_AER_CE_MEM_DATA_ECC, .error = "Memory Data ECC Error" },
	{ .bit = CXL_AER_CE_CRC_THRESH, .error = "CRC Threshold Hit" },
	{ .bit = CXL_AER_CE_RETRY_THRESH, .error = "Retry Threshold" },
	{ .bit = CXL_AER_CE_CACHE_POISON, .error = "Received Cache Poison From Peer" },
	{ .bit = CXL_AER_CE_MEM_POISON, .error = "Received Memory Poison From Peer" },
	{ .bit = CXL_AER_CE_PHYS_LAYER_ERR, .error = "Received Error From Physical Layer" },
};

static int decode_cxl_error_status(struct trace_seq *s, uint32_t status,
				   const struct cxl_error_list *cxl_error_list,
				   uint8_t num_elems)
{
	int i;

	for (i = 0; i < num_elems; i++) {
		if (status & cxl_error_list[i].bit)
			if (trace_seq_printf(s, "\'%s\' ", cxl_error_list[i].error) <= 0)
				return -1;
	}
	return 0;
}

int ras_cxl_aer_ue_event_handler(struct trace_seq *s,
				 struct tep_record *record,
				 struct tep_event *event, void *context)
{
	int len, i;
	unsigned long long val;
	struct ras_events *ras = context;
	struct ras_cxl_aer_ue_event ev;

	memset(&ev, 0, sizeof(ev));
	get_timestamp(s, record, ras, (char *)&ev.timestamp, sizeof(ev.timestamp));
	if (trace_seq_printf(s, "%s ", ev.timestamp) <= 0)
		return -1;

	ev.memdev = tep_get_field_raw(s, event, "memdev",
				      record, &len, 1);
	if (!ev.memdev)
		return -1;
	if (trace_seq_printf(s, "memdev:%s ", ev.memdev) <= 0)
		return -1;

	ev.host = tep_get_field_raw(s, event, "host",
				    record, &len, 1);
	if (!ev.host)
		return -1;
	if (trace_seq_printf(s, "host:%s ", ev.host) <= 0)
		return -1;

	if (tep_get_field_val(s, event, "serial", record, &val, 1) < 0)
		return -1;
	ev.serial = val;
	if (trace_seq_printf(s, "serial:0x%llx ", (unsigned long long)ev.serial) <= 0)
		return -1;

	if (tep_get_field_val(s, event, "status", record, &val, 1) < 0)
		return -1;
	ev.error_status = val;

	if (trace_seq_printf(s, "error status:") <= 0)
		return -1;
	if (decode_cxl_error_status(s, ev.error_status,
				    cxl_aer_ue, ARRAY_SIZE(cxl_aer_ue)) < 0)
		return -1;

	if (tep_get_field_val(s,  event, "first_error", record, &val, 1) < 0)
		return -1;
	ev.first_error = val;

	if (trace_seq_printf(s, "first error:") <= 0)
		return -1;
	if (decode_cxl_error_status(s, ev.first_error,
				    cxl_aer_ue, ARRAY_SIZE(cxl_aer_ue)) < 0)
		return -1;

	ev.header_log = tep_get_field_raw(s, event, "header_log",
					  record, &len, 1);
	if (!ev.header_log)
		return -1;
	if (trace_seq_printf(s, "header log:\n") <= 0)
		return -1;
	for (i = 0; i < CXL_HEADERLOG_SIZE_U32; i++) {
		if (trace_seq_printf(s, "%08x ", ev.header_log[i]) <= 0)
			break;
		if (i > 0 && ((i % 20) == 0))
			if (trace_seq_printf(s, "\n") <= 0)
				break;
		/* Convert header log data to the big-endian format because
		 * the SQLite database seems uses the big-endian storage.
		 */
		ev.header_log[i] = htobe32(ev.header_log[i]);
	}
	if (i < CXL_HEADERLOG_SIZE_U32)
		return -1;

	/* Insert data into the SGBD */
#ifdef HAVE_SQLITE3
	ras_store_cxl_aer_ue_event(ras, &ev);
#endif

#ifdef HAVE_ABRT_REPORT
	/* Report event to ABRT */
	ras_report_cxl_aer_ue_event(ras, &ev);
#endif

	return 0;
}

int ras_cxl_aer_ce_event_handler(struct trace_seq *s,
				 struct tep_record *record,
				 struct tep_event *event, void *context)
{
	int len;
	unsigned long long val;
	struct ras_events *ras = context;
	struct ras_cxl_aer_ce_event ev;

	get_timestamp(s, record, ras, (char *)&ev.timestamp, sizeof(ev.timestamp));
	if (trace_seq_printf(s, "%s ", ev.timestamp) <= 0)
		return -1;

	ev.memdev = tep_get_field_raw(s, event, "memdev",
				      record, &len, 1);
	if (!ev.memdev)
		return -1;
	if (trace_seq_printf(s, "memdev:%s ", ev.memdev) <= 0)
		return -1;

	ev.host = tep_get_field_raw(s, event, "host",
				    record, &len, 1);
	if (!ev.host)
		return -1;
	if (trace_seq_printf(s, "host:%s ", ev.host) <= 0)
		return -1;

	if (tep_get_field_val(s, event, "serial", record, &val, 1) < 0)
		return -1;
	ev.serial = val;
	if (trace_seq_printf(s, "serial:0x%llx ", (unsigned long long)ev.serial) <= 0)
		return -1;

	if (tep_get_field_val(s, event, "status", record, &val, 1) < 0)
		return -1;
	ev.error_status = val;
	if (trace_seq_printf(s, "error status:") <= 0)
		return -1;
	if (decode_cxl_error_status(s, ev.error_status,
				    cxl_aer_ce, ARRAY_SIZE(cxl_aer_ce)) < 0)
		return -1;

	/* Insert data into the SGBD */
#ifdef HAVE_SQLITE3
	ras_store_cxl_aer_ce_event(ras, &ev);
#endif

#ifdef HAVE_ABRT_REPORT
	/* Report event to ABRT */
	ras_report_cxl_aer_ce_event(ras, &ev);
#endif

	return 0;
}

/*
 * CXL rev 3.0 section 8.2.9.2.2; Table 8-49
 */
enum cxl_event_log_type {
	CXL_EVENT_TYPE_INFO = 0x00,
	CXL_EVENT_TYPE_WARN,
	CXL_EVENT_TYPE_FAIL,
	CXL_EVENT_TYPE_FATAL,
	CXL_EVENT_TYPE_UNKNOWN
};

static char *cxl_event_log_type_str(uint32_t log_type)
{
	switch (log_type) {
	case CXL_EVENT_TYPE_INFO:
		return "Informational";
	case CXL_EVENT_TYPE_WARN:
		return "Warning";
	case CXL_EVENT_TYPE_FAIL:
		return "Failure";
	case CXL_EVENT_TYPE_FATAL:
		return "Fatal";
	default:
		break;
	}

	return "Unknown";
}

int ras_cxl_overflow_event_handler(struct trace_seq *s,
				   struct tep_record *record,
				   struct tep_event *event, void *context)
{
	int len;
	unsigned long long val;
	struct ras_events *ras = context;
	struct ras_cxl_overflow_event ev;

	memset(&ev, 0, sizeof(ev));
	get_timestamp(s, record, ras, (char *)&ev.timestamp, sizeof(ev.timestamp));
	if (trace_seq_printf(s, "%s ", ev.timestamp) <= 0)
		return -1;

	ev.memdev = tep_get_field_raw(s, event, "memdev", record, &len, 1);
	if (!ev.memdev)
		return -1;
	if (trace_seq_printf(s, "memdev:%s ", ev.memdev) <= 0)
		return -1;

	ev.host = tep_get_field_raw(s, event, "host", record, &len, 1);
	if (!ev.host)
		return -1;
	if (trace_seq_printf(s, "host:%s ", ev.host) <= 0)
		return -1;

	if (tep_get_field_val(s, event, "serial", record, &val, 1) < 0)
		return -1;
	ev.serial = val;
	if (trace_seq_printf(s, "serial:0x%llx ", (unsigned long long)ev.serial) <= 0)
		return -1;

	if (tep_get_field_val(s, event, "log", record, &val, 1) < 0)
		return -1;
	ev.log_type = cxl_event_log_type_str(val);
	if (trace_seq_printf(s, "log type:%s ", ev.log_type) <= 0)
		return -1;

	if (tep_get_field_val(s, event, "count", record, &val, 1) < 0)
		return -1;
	ev.count = val;

	if (tep_get_field_val(s,  event, "first_ts", record, &val, 1) < 0)
		return -1;
	convert_timestamp(val, ev.first_ts, sizeof(ev.first_ts));

	if (tep_get_field_val(s,  event, "last_ts", record, &val, 1) < 0)
		return -1;
	convert_timestamp(val, ev.last_ts, sizeof(ev.last_ts));

	if (ev.count) {
		if (trace_seq_printf(s, "%u errors from %s to %s\n",
				     ev.count, ev.first_ts, ev.last_ts) <= 0)
			return -1;
	}
	/* Insert data into the SGBD */
#ifdef HAVE_SQLITE3
	ras_store_cxl_overflow_event(ras, &ev);
#endif

#ifdef HAVE_ABRT_REPORT
	/* Report event to ABRT */
	ras_report_cxl_overflow_event(ras, &ev);
#endif

	return 0;
}

/*
 * Common Event Record Format
 * CXL 3.0 section 8.2.9.2.1; Table 8-42
 */
#define CXL_EVENT_RECORD_FLAG_PERMANENT		BIT(2)
#define CXL_EVENT_RECORD_FLAG_MAINT_NEEDED	BIT(3)
#define CXL_EVENT_RECORD_FLAG_PERF_DEGRADED	BIT(4)
#define CXL_EVENT_RECORD_FLAG_HW_REPLACE	BIT(5)

static const struct  cxl_event_flags cxl_hdr_flags[] = {
	{ .bit = CXL_EVENT_RECORD_FLAG_PERMANENT, .flag = "PERMANENT_CONDITION" },
	{ .bit = CXL_EVENT_RECORD_FLAG_MAINT_NEEDED, .flag = "MAINTENANCE_NEEDED" },
	{ .bit = CXL_EVENT_RECORD_FLAG_PERF_DEGRADED, .flag = "PERFORMANCE_DEGRADED" },
	{ .bit = CXL_EVENT_RECORD_FLAG_HW_REPLACE, .flag = "HARDWARE_REPLACEMENT_NEEDED" },
};

static int handle_ras_cxl_common_hdr(struct trace_seq *s,
				     struct tep_record *record,
				     struct tep_event *event, void *context,
				     struct ras_cxl_event_common_hdr *hdr)
{
	int len;
	unsigned long long val;
	struct ras_events *ras = context;

	get_timestamp(s, record, ras, (char *)&hdr->timestamp, sizeof(hdr->timestamp));
	if (trace_seq_printf(s, "%s ", hdr->timestamp) <= 0)
		return -1;

	hdr->memdev = tep_get_field_raw(s, event, "memdev", record, &len, 1);
	if (!hdr->memdev)
		return -1;
	if (trace_seq_printf(s, "memdev:%s ", hdr->memdev) <= 0)
		return -1;

	hdr->host = tep_get_field_raw(s, event, "host", record, &len, 1);
	if (!hdr->host)
		return -1;
	if (trace_seq_printf(s, "host:%s ", hdr->host) <= 0)
		return -1;

	if (tep_get_field_val(s, event, "serial", record, &val, 1) < 0)
		return -1;
	hdr->serial = val;
	if (trace_seq_printf(s, "serial:0x%llx ", (unsigned long long)hdr->serial) <= 0)
		return -1;

	if (tep_get_field_val(s, event, "log", record, &val, 1) < 0)
		return -1;
	hdr->log_type = cxl_event_log_type_str(val);
	if (trace_seq_printf(s, "log type:%s ", hdr->log_type) <= 0)
		return -1;

	hdr->hdr_uuid = tep_get_field_raw(s, event, "hdr_uuid", record, &len, 1);
	if (!hdr->hdr_uuid)
		return -1;
	hdr->hdr_uuid = uuid_be(hdr->hdr_uuid);
	if (trace_seq_printf(s, "hdr_uuid:%s ", hdr->hdr_uuid) <= 0)
		return -1;

	if (tep_get_field_val(s, event, "hdr_flags", record, &val, 1) < 0)
		return -1;
	hdr->hdr_flags = val;
	if (decode_cxl_event_flags(s, hdr->hdr_flags, cxl_hdr_flags,
				   ARRAY_SIZE(cxl_hdr_flags)) < 0)
		return -1;

	if (tep_get_field_val(s, event, "hdr_handle", record, &val, 1) < 0)
		return -1;
	hdr->hdr_handle = val;
	if (trace_seq_printf(s, "hdr_handle:0x%x ", hdr->hdr_handle) <= 0)
		return -1;

	if (tep_get_field_val(s, event, "hdr_related_handle", record, &val, 1) < 0)
		return -1;
	hdr->hdr_related_handle = val;
	if (trace_seq_printf(s, "hdr_related_handle:0x%x ", hdr->hdr_related_handle) <= 0)
		return -1;

	if (tep_get_field_val(s,  event, "hdr_timestamp", record, &val, 1) < 0)
		return -1;
	convert_timestamp(val, hdr->hdr_timestamp, sizeof(hdr->hdr_timestamp));
	if (trace_seq_printf(s, "hdr_timestamp:%s ", hdr->hdr_timestamp) <= 0)
		return -1;

	if (tep_get_field_val(s,  event, "hdr_length", record, &val, 1) < 0)
		return -1;
	hdr->hdr_length = val;
	if (trace_seq_printf(s, "hdr_length:%u ", hdr->hdr_length) <= 0)
		return -1;

	if (tep_get_field_val(s,  event, "hdr_maint_op_class", record, &val, 1) < 0)
		return -1;
	hdr->hdr_maint_op_class = val;
	if (trace_seq_printf(s, "hdr_maint_op_class:%u ", hdr->hdr_maint_op_class) <= 0)
		return -1;

	return 0;
}

int ras_cxl_generic_event_handler(struct trace_seq *s,
				  struct tep_record *record,
				  struct tep_event *event, void *context)
{
	int len, i;
	struct ras_events *ras = context;
	struct ras_cxl_generic_event ev;
	const uint8_t *buf;

	memset(&ev, 0, sizeof(ev));
	if (handle_ras_cxl_common_hdr(s, record, event, context, &ev.hdr) < 0)
		return -1;

	ev.data = tep_get_field_raw(s, event, "data", record, &len, 1);
	if (!ev.data)
		return -1;
	i = 0;
	buf = ev.data;
	if (trace_seq_printf(s, "\ndata:\n  %08x: ", i) <= 0)
		return -1;
	for (i = 0; i < CXL_EVENT_RECORD_DATA_LENGTH; i += 4) {
		if (i > 0 && ((i % 16) == 0))
			if (trace_seq_printf(s, "\n  %08x: ", i) <= 0)
				break;
		if (trace_seq_printf(s, "%02x%02x%02x%02x ",
				     buf[i], buf[i + 1], buf[i + 2], buf[i + 3]) <= 0)
			break;
	}

	/* Insert data into the SGBD */
#ifdef HAVE_SQLITE3
	ras_store_cxl_generic_event(ras, &ev);
#endif

#ifdef HAVE_ABRT_REPORT
	/* Report event to ABRT */
	ras_report_cxl_generic_event(ras, &ev);
#endif

	return 0;
}

#define CXL_DPA_VOLATILE		BIT(0)
#define CXL_DPA_NOT_REPAIRABLE		BIT(1)

static const struct cxl_event_flags cxl_dpa_flags[] = {
	{ .bit = CXL_DPA_VOLATILE, .flag = "VOLATILE" },
	{ .bit = CXL_DPA_NOT_REPAIRABLE, .flag = "NOT_REPAIRABLE" },
};

/*
 * General Media Event Record - GMER
 * CXL rev 3.0 Section 8.2.9.2.1.1; Table 8-43
 */
#define CXL_GMER_EVT_DESC_UNCORRECTABLE_EVENT		BIT(0)
#define CXL_GMER_EVT_DESC_THRESHOLD_EVENT		BIT(1)
#define CXL_GMER_EVT_DESC_POISON_LIST_OVERFLOW		BIT(2)

static const struct cxl_event_flags cxl_gmer_event_desc_flags[] = {
	{ .bit = CXL_GMER_EVT_DESC_UNCORRECTABLE_EVENT, .flag = "UNCORRECTABLE EVENT" },
	{ .bit = CXL_GMER_EVT_DESC_THRESHOLD_EVENT, .flag = "THRESHOLD EVENT" },
	{ .bit = CXL_GMER_EVT_DESC_POISON_LIST_OVERFLOW, .flag = "POISON LIST OVERFLOW" },
};

#define CXL_GMER_VALID_CHANNEL			BIT(0)
#define CXL_GMER_VALID_RANK			BIT(1)
#define CXL_GMER_VALID_DEVICE			BIT(2)
#define CXL_GMER_VALID_COMPONENT		BIT(3)

static const char * const cxl_gmer_mem_event_type[] = {
	"ECC Error",
	"Invalid Address",
	"Data Path Error",
};

static const char * const cxl_gmer_trans_type[] = {
	"Unknown",
	"Host Read",
	"Host Write",
	"Host Scan Media",
	"Host Inject Poison",
	"Internal Media Scrub",
	"Internal Media Management",
};

int ras_cxl_general_media_event_handler(struct trace_seq *s,
					struct tep_record *record,
					struct tep_event *event, void *context)
{
	int len, i;
	unsigned long long val;
	struct ras_events *ras = context;
	struct ras_cxl_general_media_event ev;

	memset(&ev, 0, sizeof(ev));
	if (handle_ras_cxl_common_hdr(s, record, event, context, &ev.hdr) < 0)
		return -1;

	if (tep_get_field_val(s, event, "dpa", record, &val, 1) < 0)
		return -1;
	ev.dpa = val;
	if (trace_seq_printf(s, "dpa:0x%llx ", (unsigned long long)ev.dpa) <= 0)
		return -1;

	if (tep_get_field_val(s,  event, "dpa_flags", record, &val, 1) < 0)
		return -1;
	ev.dpa_flags = val;
	if (trace_seq_printf(s, "dpa_flags:") <= 0)
		return -1;
	if (decode_cxl_event_flags(s, ev.dpa_flags, cxl_dpa_flags, ARRAY_SIZE(cxl_dpa_flags)) < 0)
		return -1;

	if (tep_get_field_val(s,  event, "descriptor", record, &val, 1) < 0)
		return -1;
	ev.descriptor = val;
	if (trace_seq_printf(s, "descriptor:") <= 0)
		return -1;
	if (decode_cxl_event_flags(s, ev.descriptor, cxl_gmer_event_desc_flags,
				   ARRAY_SIZE(cxl_gmer_event_desc_flags)) < 0)
		return -1;

	if (tep_get_field_val(s,  event, "type", record, &val, 1) < 0)
		return -1;
	ev.type = val;
	if (trace_seq_printf(s, "type:%s ",
			     get_cxl_type_str(cxl_gmer_mem_event_type,
					      ARRAY_SIZE(cxl_gmer_mem_event_type), ev.type)) <= 0)
		return -1;

	if (tep_get_field_val(s,  event, "transaction_type", record, &val, 1) < 0)
		return -1;
	ev.transaction_type = val;
	if (trace_seq_printf(s, "transaction_type:%s ",
			     get_cxl_type_str(cxl_gmer_trans_type,
					      ARRAY_SIZE(cxl_gmer_trans_type),
					      ev.transaction_type)) <= 0)
		return -1;

	if (tep_get_field_val(s, event, "hpa", record, &val, 1) < 0)
		return -1;
	ev.hpa = val;
	if (trace_seq_printf(s, "hpa:0x%llx ", (unsigned long long)ev.hpa) <= 0)
		return -1;

	ev.region = tep_get_field_raw(s, event, "region_name", record, &len, 1);
	if (!ev.region)
		return -1;
	if (trace_seq_printf(s, "region:%s ", ev.region) <= 0)
		return -1;

	ev.region_uuid = tep_get_field_raw(s, event, "region_uuid",
					   record, &len, 1);
	if (!ev.region_uuid)
		return -1;
	ev.region_uuid = uuid_be(ev.region_uuid);
	if (trace_seq_printf(s, "region_uuid:%s ", ev.region_uuid) <= 0)
		return -1;

	if (tep_get_field_val(s,  event, "validity_flags", record, &val, 1) < 0)
		return -1;
	ev.validity_flags = val;

	if (ev.validity_flags & CXL_GMER_VALID_CHANNEL) {
		if (tep_get_field_val(s,  event, "channel", record, &val, 1) < 0)
			return -1;
		ev.channel = val;
		if (trace_seq_printf(s, "channel:%u ", ev.channel) <= 0)
			return -1;
	}

	if (ev.validity_flags & CXL_GMER_VALID_RANK) {
		if (tep_get_field_val(s,  event, "rank", record, &val, 1) < 0)
			return -1;
		ev.rank = val;
		if (trace_seq_printf(s, "rank:%u ", ev.rank) <= 0)
			return -1;
	}

	if (ev.validity_flags & CXL_GMER_VALID_DEVICE) {
		if (tep_get_field_val(s,  event, "device", record, &val, 1) < 0)
			return -1;
		ev.device = val;
		if (trace_seq_printf(s, "device:%x ", ev.device) <= 0)
			return -1;
	}

	if (ev.validity_flags & CXL_GMER_VALID_COMPONENT) {
		ev.comp_id = tep_get_field_raw(s, event, "comp_id", record, &len, 1);
		if (!ev.comp_id)
			return -1;
		if (trace_seq_printf(s, "comp_id:") <= 0)
			return -1;
		for (i = 0; i < CXL_EVENT_GEN_MED_COMP_ID_SIZE; i++) {
			if (trace_seq_printf(s, "%02x ", ev.comp_id[i]) <= 0)
				break;
		}
	}

	/* Insert data into the SGBD */
#ifdef HAVE_SQLITE3
	ras_store_cxl_general_media_event(ras, &ev);
#endif

#ifdef HAVE_ABRT_REPORT
	/* Report event to ABRT */
	ras_report_cxl_general_media_event(ras, &ev);
#endif

	return 0;
}

/*
 * DRAM Event Record - DER
 *
 * CXL rev 3.0 section 8.2.9.2.1.2; Table 8-44
 */
#define CXL_DER_VALID_CHANNEL			BIT(0)
#define CXL_DER_VALID_RANK			BIT(1)
#define CXL_DER_VALID_NIBBLE			BIT(2)
#define CXL_DER_VALID_BANK_GROUP		BIT(3)
#define CXL_DER_VALID_BANK			BIT(4)
#define CXL_DER_VALID_ROW			BIT(5)
#define CXL_DER_VALID_COLUMN			BIT(6)
#define CXL_DER_VALID_CORRECTION_MASK		BIT(7)

int ras_cxl_dram_event_handler(struct trace_seq *s,
			       struct tep_record *record,
			       struct tep_event *event, void *context)
{
	int len, i;
	unsigned long long val;
	struct ras_events *ras = context;
	struct ras_cxl_dram_event ev;

	memset(&ev, 0, sizeof(ev));
	if (handle_ras_cxl_common_hdr(s, record, event, context, &ev.hdr) < 0)
		return -1;

	if (tep_get_field_val(s, event, "dpa", record, &val, 1) < 0)
		return -1;
	ev.dpa = val;
	if (trace_seq_printf(s, "dpa:0x%llx ", (unsigned long long)ev.dpa) <= 0)
		return -1;

	if (tep_get_field_val(s, event, "hpa", record, &val, 1) < 0)
		return -1;
	ev.hpa = val;
	if (trace_seq_printf(s, "hpa:0x%llx ", (unsigned long long)ev.hpa) <= 0)
		return -1;

	if (tep_get_field_val(s,  event, "dpa_flags", record, &val, 1) < 0)
		return -1;
	ev.dpa_flags = val;
	if (trace_seq_printf(s, "dpa_flags:") <= 0)
		return -1;
	if (decode_cxl_event_flags(s, ev.dpa_flags, cxl_dpa_flags,
				   ARRAY_SIZE(cxl_dpa_flags)) < 0)
		return -1;

	if (tep_get_field_val(s,  event, "descriptor", record, &val, 1) < 0)
		return -1;
	ev.descriptor = val;
	if (trace_seq_printf(s, "descriptor:") <= 0)
		return -1;
	if (decode_cxl_event_flags(s, ev.descriptor, cxl_gmer_event_desc_flags,
				   ARRAY_SIZE(cxl_gmer_event_desc_flags)) < 0)
		return -1;

	if (tep_get_field_val(s,  event, "type", record, &val, 1) < 0)
		return -1;
	ev.type = val;
	if (trace_seq_printf(s, "type:%s ",
			     get_cxl_type_str(cxl_gmer_mem_event_type,
					      ARRAY_SIZE(cxl_gmer_mem_event_type),
					      ev.type)) <= 0)
		return -1;

	if (tep_get_field_val(s,  event, "transaction_type", record, &val, 1) < 0)
		return -1;
	ev.transaction_type = val;
	if (trace_seq_printf(s, "transaction_type:%s ",
			     get_cxl_type_str(cxl_gmer_trans_type,
					      ARRAY_SIZE(cxl_gmer_trans_type),
					      ev.transaction_type)) <= 0)
		return -1;

	if (tep_get_field_val(s, event, "hpa", record, &val, 1) < 0)
		return -1;
	ev.hpa = val;
	if (trace_seq_printf(s, "hpa:0x%llx ", (unsigned long long)ev.hpa) <= 0)
		return -1;

	ev.region = tep_get_field_raw(s, event, "region", record, &len, 1);
	if (!ev.region)
		return -1;
	if (trace_seq_printf(s, "region:%s ", ev.region) <= 0)
		return -1;

	ev.region_uuid = tep_get_field_raw(s, event, "region_uuid",
					   record, &len, 1);
	if (!ev.region_uuid)
		return -1;
	ev.region_uuid = uuid_be(ev.region_uuid);
	if (trace_seq_printf(s, "region_uuid:%s ", ev.region_uuid) <= 0)
		return -1;

	if (tep_get_field_val(s,  event, "validity_flags", record, &val, 1) < 0)
		return -1;
	ev.validity_flags = val;

	if (ev.validity_flags & CXL_DER_VALID_CHANNEL) {
		if (tep_get_field_val(s,  event, "channel", record, &val, 1) < 0)
			return -1;
		ev.channel = val;
		if (trace_seq_printf(s, "channel:%u ", ev.channel) <= 0)
			return -1;
	}

	if (ev.validity_flags & CXL_DER_VALID_RANK) {
		if (tep_get_field_val(s,  event, "rank", record, &val, 1) < 0)
			return -1;
		ev.rank = val;
		if (trace_seq_printf(s, "rank:%u ", ev.rank) <= 0)
			return -1;
	}

	if (ev.validity_flags & CXL_DER_VALID_NIBBLE) {
		if (tep_get_field_val(s,  event, "nibble_mask", record, &val, 1) < 0)
			return -1;
		ev.nibble_mask = val;
		if (trace_seq_printf(s, "nibble_mask:%u ", ev.nibble_mask) <= 0)
			return -1;
	}

	if (ev.validity_flags & CXL_DER_VALID_BANK_GROUP) {
		if (tep_get_field_val(s,  event, "bank_group", record, &val, 1) < 0)
			return -1;
		ev.bank_group = val;
		if (trace_seq_printf(s, "bank_group:%u ", ev.bank_group) <= 0)
			return -1;
	}

	if (ev.validity_flags & CXL_DER_VALID_BANK) {
		if (tep_get_field_val(s,  event, "bank", record, &val, 1) < 0)
			return -1;
		ev.bank = val;
		if (trace_seq_printf(s, "bank:%u ", ev.bank) <= 0)
			return -1;
	}

	if (ev.validity_flags & CXL_DER_VALID_ROW) {
		if (tep_get_field_val(s,  event, "row", record, &val, 1) < 0)
			return -1;
		ev.row = val;
		if (trace_seq_printf(s, "row:%u ", ev.row) <= 0)
			return -1;
	}

	if (ev.validity_flags & CXL_DER_VALID_COLUMN) {
		if (tep_get_field_val(s,  event, "column", record, &val, 1) < 0)
			return -1;
		ev.column = val;
		if (trace_seq_printf(s, "column:%u ", ev.column) <= 0)
			return -1;
	}

	if (ev.validity_flags & CXL_DER_VALID_CORRECTION_MASK) {
		ev.cor_mask = tep_get_field_raw(s, event, "cor_mask", record, &len, 1);
		if (!ev.cor_mask)
			return -1;
		if (trace_seq_printf(s, "correction_mask:") <= 0)
			return -1;
		for (i = 0; i < CXL_EVENT_DER_CORRECTION_MASK_SIZE; i++) {
			if (trace_seq_printf(s, "%02x ", ev.cor_mask[i]) <= 0)
				break;
		}
	}

#ifdef HAVE_MEMORY_CE_PFA
	/* Page offline for CE when threeshold is set */
	if (!(ev.descriptor & CXL_GMER_EVT_DESC_UNCORRECTABLE_EVENT) &&
	    (ev.descriptor & CXL_GMER_EVT_DESC_THRESHOLD_EVENT))
		ras_hw_threshold_pageoffline(ev.hpa);
#endif

	/* Insert data into the SGBD */
#ifdef HAVE_SQLITE3
	ras_store_cxl_dram_event(ras, &ev);
#endif

#ifdef HAVE_ABRT_REPORT
	/* Report event to ABRT */
	ras_report_cxl_dram_event(ras, &ev);
#endif

	return 0;
}

/*
 * Memory Module Event Record - MMER
 *
 * CXL res 3.0 section 8.2.9.2.1.3; Table 8-45
 */
static const char * const cxl_dev_evt_type[] = {
	"Health Status Change",
	"Media Status Change",
	"Life Used Change",
	"Temperature Change",
	"Data Path Error",
	"LSA Error",
};

/*
 * Device Health Information - DHI
 *
 * CXL res 3.0 section 8.2.9.8.3.1; Table 8-100
 */
#define CXL_DHI_HS_MAINTENANCE_NEEDED				BIT(0)
#define CXL_DHI_HS_PERFORMANCE_DEGRADED				BIT(1)
#define CXL_DHI_HS_HW_REPLACEMENT_NEEDED			BIT(2)

static const struct cxl_event_flags cxl_health_status[] = {
	{ .bit = CXL_DHI_HS_MAINTENANCE_NEEDED, .flag = "MAINTENANCE_NEEDED" },
	{ .bit = CXL_DHI_HS_PERFORMANCE_DEGRADED, .flag = "PERFORMANCE_DEGRADED" },
	{ .bit = CXL_DHI_HS_HW_REPLACEMENT_NEEDED, .flag = "REPLACEMENT_NEEDED" },
};

static const char * const cxl_media_status[] = {
	"Normal",
	"Not Ready",
	"Write Persistency Lost",
	"All Data Lost",
	"Write Persistency Loss in the Event of Power Loss",
	"Write Persistency Loss in Event of Shutdown",
	"Write Persistency Loss Imminent",
	"All Data Loss in Event of Power Loss",
	"All Data loss in the Event of Shutdown",
	"All Data Loss Imminent",
};

static const char * const cxl_two_bit_status[] = {
	"Normal",
	"Warning",
	"Critical",
};

static const char * const cxl_one_bit_status[] = {
	"Normal",
	"Warning",
};

#define CXL_DHI_AS_LIFE_USED(as)	((as) & 0x3)
#define CXL_DHI_AS_DEV_TEMP(as)		(((as) & 0xC) >> 2)
#define CXL_DHI_AS_COR_VOL_ERR_CNT(as)	(((as) & 0x10) >> 4)
#define CXL_DHI_AS_COR_PER_ERR_CNT(as)	(((as) & 0x20) >> 5)

int ras_cxl_memory_module_event_handler(struct trace_seq *s,
					struct tep_record *record,
					struct tep_event *event, void *context)
{
	unsigned long long val;
	struct ras_events *ras = context;
	struct ras_cxl_memory_module_event ev;

	memset(&ev, 0, sizeof(ev));
	if (handle_ras_cxl_common_hdr(s, record, event, context, &ev.hdr) < 0)
		return -1;

	if (tep_get_field_val(s, event, "event_type", record, &val, 1) < 0)
		return -1;
	ev.event_type = val;
	if (trace_seq_printf(s, "event_type:%s ",
			     get_cxl_type_str(cxl_dev_evt_type,
					      ARRAY_SIZE(cxl_dev_evt_type),
					      ev.event_type)) <= 0)
		return -1;

	if (tep_get_field_val(s, event, "health_status", record, &val, 1) < 0)
		return -1;
	ev.health_status = val;
	if (trace_seq_printf(s, "health_status:") <= 0)
		return -1;
	if (decode_cxl_event_flags(s, ev.health_status, cxl_health_status,
				   ARRAY_SIZE(cxl_health_status)) < 0)
		return -1;

	if (tep_get_field_val(s, event, "media_status", record, &val, 1) < 0)
		return -1;
	ev.media_status = val;
	if (trace_seq_printf(s, "media_status:%s ",
			     get_cxl_type_str(cxl_media_status,
					      ARRAY_SIZE(cxl_media_status),
					      ev.media_status)) <= 0)
		return -1;

	if (tep_get_field_val(s, event, "add_status", record, &val, 1) < 0)
		return -1;
	ev.add_status = val;
	if (trace_seq_printf(s, "as_life_used:%s ",
			     get_cxl_type_str(cxl_two_bit_status,
					      ARRAY_SIZE(cxl_two_bit_status),
			     CXL_DHI_AS_LIFE_USED(ev.add_status))) <= 0)
		return -1;
	if (trace_seq_printf(s, "as_dev_temp:%s ",
			     get_cxl_type_str(cxl_two_bit_status,
					      ARRAY_SIZE(cxl_two_bit_status),
			     CXL_DHI_AS_DEV_TEMP(ev.add_status))) <= 0)
		return -1;
	if (trace_seq_printf(s, "as_cor_vol_err_cnt:%s ",
			     get_cxl_type_str(cxl_one_bit_status,
					      ARRAY_SIZE(cxl_one_bit_status),
			     CXL_DHI_AS_COR_VOL_ERR_CNT(ev.add_status))) <= 0)
		return -1;
	if (trace_seq_printf(s, "as_cor_per_err_cnt:%s ",
			     get_cxl_type_str(cxl_one_bit_status,
					      ARRAY_SIZE(cxl_one_bit_status),
			     CXL_DHI_AS_COR_PER_ERR_CNT(ev.add_status))) <= 0)
		return -1;

	if (tep_get_field_val(s, event, "life_used", record, &val, 1) < 0)
		return -1;
	ev.life_used = val;
	if (trace_seq_printf(s, "life_used:%u ", ev.life_used) <= 0)
		return -1;

	if (tep_get_field_val(s, event, "device_temp", record, &val, 1) < 0)
		return -1;
	ev.device_temp = val;
	if (trace_seq_printf(s, "device_temp:%u ", ev.device_temp) <= 0)
		return -1;

	if (tep_get_field_val(s, event, "dirty_shutdown_cnt", record, &val, 1) < 0)
		return -1;
	ev.dirty_shutdown_cnt = val;
	if (trace_seq_printf(s, "dirty_shutdown_cnt:%u ", ev.dirty_shutdown_cnt) <= 0)
		return -1;

	if (tep_get_field_val(s, event, "cor_vol_err_cnt", record, &val, 1) < 0)
		return -1;
	ev.cor_vol_err_cnt = val;
	if (trace_seq_printf(s, "cor_vol_err_cnt:%u ", ev.cor_vol_err_cnt) <= 0)
		return -1;

	if (tep_get_field_val(s, event, "cor_per_err_cnt", record, &val, 1) < 0)
		return -1;
	ev.cor_per_err_cnt = val;
	if (trace_seq_printf(s, "cor_per_err_cnt:%u ", ev.cor_per_err_cnt) <= 0)
		return -1;

	/* Insert data into the SGBD */
#ifdef HAVE_SQLITE3
	ras_store_cxl_memory_module_event(ras, &ev);
#endif

#ifdef HAVE_ABRT_REPORT
	/* Report event to ABRT */
	ras_report_cxl_memory_module_event(ras, &ev);
#endif

	return 0;
}
