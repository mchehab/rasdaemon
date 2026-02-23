/* SPDX-License-Identifier: GPL-2.0-only */

/*
 * Copyright (c) 2026, NVIDIA CORPORATION & AFFILIATES.  All rights reserved.
 */

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "non-standard-nvidia.h"
#include "ras-logger.h"
#include "ras-non-standard-handler.h"

static const char * const nvidia_reg_names[] = {
	[NVIDIA_FIELD_SIGNATURE]     = "Signature:",
	[NVIDIA_FIELD_ERROR_TYPE]    = "Error Type:",
	[NVIDIA_FIELD_ERROR_INSTANCE]= "Error Instance:",
	[NVIDIA_FIELD_SEVERITY]      = "Severity:",
	[NVIDIA_FIELD_SOCKET]        = "Socket:",
	[NVIDIA_FIELD_NUMBER_REGS]   = "Number of Registers:",
	[NVIDIA_FIELD_INSTANCE_BASE] = "Instance Base:",
	[NVIDIA_FIELD_REG_DATA]      = "Register Data:",
};

void decode_nvidia_cper_sec(struct ras_ns_ev_decoder *ev_decoder,
			    struct trace_seq *s,
			    const struct nvidia_cper_sec *err,
			    uint32_t len)
{
	uint32_t i, reg_data_len;
	const struct nvidia_reg_pair *regs;

	trace_seq_printf(s, "%s %s\n",
			 nvidia_reg_names[NVIDIA_FIELD_SIGNATURE],
			 err->signature);
	trace_seq_printf(s, "%s %u\n",
			 nvidia_reg_names[NVIDIA_FIELD_ERROR_TYPE],
			 err->error_type);
	trace_seq_printf(s, "%s %u\n",
			 nvidia_reg_names[NVIDIA_FIELD_ERROR_INSTANCE],
			 err->error_instance);
	trace_seq_printf(s, "%s %u\n",
			 nvidia_reg_names[NVIDIA_FIELD_SEVERITY],
			 err->severity);
	trace_seq_printf(s, "%s %u\n",
			 nvidia_reg_names[NVIDIA_FIELD_SOCKET],
			 err->socket);
	trace_seq_printf(s, "%s %u\n",
			 nvidia_reg_names[NVIDIA_FIELD_NUMBER_REGS],
			 err->number_regs);
	trace_seq_printf(s, "%s 0x%llx\n",
			 nvidia_reg_names[NVIDIA_FIELD_INSTANCE_BASE],
			 (unsigned long long)err->instance_base);

	/* Calculate register data length */
	reg_data_len = len - sizeof(struct nvidia_cper_sec);

	if (err->number_regs > 0 && reg_data_len > 0) {
		uint32_t expected_size = err->number_regs * sizeof(struct nvidia_reg_pair);

		trace_seq_printf(s, "%s %u bytes (%u register pairs)\n",
				 nvidia_reg_names[NVIDIA_FIELD_REG_DATA],
				 reg_data_len, err->number_regs);

		if (reg_data_len >= expected_size) {
			regs = (const struct nvidia_reg_pair *)((const char *)err + sizeof(struct nvidia_cper_sec));

			/* Print all register pairs */
			for (i = 0; i < err->number_regs; i++) {
				trace_seq_printf(s, "  Reg[%u]: addr=0x%llx val=0x%llx\n",
						 i,
						 (unsigned long long)regs[i].address,
						 (unsigned long long)regs[i].value);
			}
		} else {
			trace_seq_printf(s, "  WARNING: Data truncated (expected %u bytes)\n",
					 expected_size);
		}
	}
}

#ifdef HAVE_SQLITE3

#include <sqlite3.h>

static const struct db_fields nvidia_ns_fields[] = {
	{ .name = "id",			.type = "INTEGER PRIMARY KEY" },
	{ .name = "timestamp",		.type = "TEXT" },
	{ .name = "signature",		.type = "TEXT" },
	{ .name = "error_type",		.type = "INTEGER" },
	{ .name = "error_instance",	.type = "INTEGER" },
	{ .name = "severity",		.type = "INTEGER" },
	{ .name = "socket",		.type = "INTEGER" },
	{ .name = "number_regs",	.type = "INTEGER" },
	{ .name = "instance_base",	.type = "INTEGER" },
	{ .name = "reg_data",		.type = "BLOB" },
	{ .name = "raw_data",		.type = "BLOB" },
};

static const struct db_table_descriptor nvidia_ns_table = {
	.name = "nvidia_ns_event",
	.fields = nvidia_ns_fields,
	.num_fields = ARRAY_SIZE(nvidia_ns_fields),
};

static int nvidia_ns_add_table(struct ras_events *ras,
			       struct ras_ns_ev_decoder *ev_decoder)
{
	int rc;

	rc = ras_mc_add_vendor_table(ras, &ev_decoder->stmt_dec_record,
				     &nvidia_ns_table);
	if (rc != SQLITE_OK)
		log(TERM, LOG_ERR, "Failed to create/prepare NVIDIA table: %d\n", rc);

	return rc;
}

static int nvidia_ns_decode(struct ras_events *ras,
			    struct ras_ns_ev_decoder *ev_decoder,
			    struct trace_seq *s,
			    struct ras_non_standard_event *event)
{
	const struct nvidia_cper_sec *err;
	char timestamp[64];
	uint32_t reg_data_len;
	time_t now;
	struct tm *tm;
	int rc;

	if (event->length < sizeof(struct nvidia_cper_sec)) {
		trace_seq_printf(s, "NVIDIA CPER section too small: %u bytes\n",
				 event->length);
		return -1;
	}

	err = (const struct nvidia_cper_sec *)event->error;

	/* Format timestamp */
	now = time(NULL);
	tm = localtime(&now);
	if (tm)
		strftime(timestamp, sizeof(timestamp),
			 "%Y-%m-%d %H:%M:%S %z", tm);
	else
		strcpy(timestamp, "unknown");

	/* Decode the CPER section for display */
	decode_nvidia_cper_sec(ev_decoder, s, err, event->length);

	/* Store in database */
	if (ev_decoder->stmt_dec_record) {
		reg_data_len = event->length - sizeof(struct nvidia_cper_sec);

		sqlite3_bind_text(ev_decoder->stmt_dec_record, 1, timestamp, -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(ev_decoder->stmt_dec_record, 2, err->signature, sizeof(err->signature), SQLITE_TRANSIENT);
		sqlite3_bind_int(ev_decoder->stmt_dec_record, 3, err->error_type);
		sqlite3_bind_int(ev_decoder->stmt_dec_record, 4, err->error_instance);
		sqlite3_bind_int(ev_decoder->stmt_dec_record, 5, err->severity);
		sqlite3_bind_int(ev_decoder->stmt_dec_record, 6, err->socket);
		sqlite3_bind_int(ev_decoder->stmt_dec_record, 7, err->number_regs);
		sqlite3_bind_int64(ev_decoder->stmt_dec_record, 8, err->instance_base);

		/* Bind register data (parsed from structure) */
		if (reg_data_len > 0) {
			const uint8_t *reg_data = (const uint8_t *)err + sizeof(struct nvidia_cper_sec);

			sqlite3_bind_blob(ev_decoder->stmt_dec_record, 9,
					  reg_data, reg_data_len, SQLITE_TRANSIENT);
		} else {
			sqlite3_bind_null(ev_decoder->stmt_dec_record, 9);
		}

		/* Bind complete raw binary data from the event */
		sqlite3_bind_blob(ev_decoder->stmt_dec_record, 10,
				  event->error, event->length, SQLITE_TRANSIENT);

		rc = sqlite3_step(ev_decoder->stmt_dec_record);
		if (rc != SQLITE_DONE)
			log(TERM, LOG_ERR,
			    "Failed to store NVIDIA event in database: error = %d\n", rc);

		rc = sqlite3_reset(ev_decoder->stmt_dec_record);
		if (rc != SQLITE_OK)
			log(TERM, LOG_ERR,
			    "Failed to reset NVIDIA statement: error = %d\n", rc);
	}

	return 0;
}

#endif  /* HAVE_SQLITE3 */

/* NVIDIA CPER decoder registration */
struct ras_ns_ev_decoder nvidia_ns_ev_decoder = {
	.sec_type = NVIDIA_SEC_TYPE_UUID,
#ifdef HAVE_SQLITE3
	.add_table = nvidia_ns_add_table,
	.decode = nvidia_ns_decode,
#endif
};

static void __attribute__((constructor)) nvidia_init(void)
{
	register_ns_ev_decoder(&nvidia_ns_ev_decoder);
}
