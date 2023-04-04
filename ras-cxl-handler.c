/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
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
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <traceevent/kbuffer.h>
#include "ras-cxl-handler.h"
#include "ras-record.h"
#include "ras-logger.h"
#include "ras-report.h"
#include <endian.h>

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
		strncpy(ts_ptr, "1970-01-01 00:00:00 +0000",
			size);
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
	time_t now;
	struct tm *tm;
	struct ras_cxl_poison_event ev;

	now = record->ts / user_hz + ras->uptime_diff;
	tm = localtime(&now);
	if (tm)
		strftime(ev.timestamp, sizeof(ev.timestamp),
			 "%Y-%m-%d %H:%M:%S %z", tm);
	else
		strncpy(ev.timestamp, "1970-01-01 00:00:00 +0000", sizeof(ev.timestamp));
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
	} else
		strncpy(ev.overflow_ts, "1970-01-01 00:00:00 +0000", sizeof(ev.overflow_ts));
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
	time_t now;
	struct tm *tm;
	struct ras_events *ras = context;
	struct ras_cxl_aer_ue_event ev;

	memset(&ev, 0, sizeof(ev));
	now = record->ts / user_hz + ras->uptime_diff;
	tm = localtime(&now);
	if (tm)
		strftime(ev.timestamp, sizeof(ev.timestamp),
			 "%Y-%m-%d %H:%M:%S %z", tm);
	else
		strncpy(ev.timestamp, "1970-01-01 00:00:00 +0000", sizeof(ev.timestamp));
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
		if ((i > 0) && ((i % 20) == 0))
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
	time_t now;
	struct tm *tm;
	struct ras_events *ras = context;
	struct ras_cxl_aer_ce_event ev;

	now = record->ts / user_hz + ras->uptime_diff;
	tm = localtime(&now);
	if (tm)
		strftime(ev.timestamp, sizeof(ev.timestamp),
			 "%Y-%m-%d %H:%M:%S %z", tm);
	else
		strncpy(ev.timestamp, "1970-01-01 00:00:00 +0000", sizeof(ev.timestamp));
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
