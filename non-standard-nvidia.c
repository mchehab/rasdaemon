/* SPDX-License-Identifier: GPL-2.0-only */

/*
 * Copyright (c) 2026, NVIDIA CORPORATION & AFFILIATES.  All rights reserved.
 */

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

#include "non-standard-nvidia.h"
#include "ras-logger.h"
#include "ras-non-standard-handler.h"

static void nvidia_format_timestamp(char *timestamp, size_t len)
{
	time_t now;
	struct tm *tm;

	if (!timestamp || !len)
		return;

	now = time(NULL);
	tm = localtime(&now);
	if (tm)
		strftime(timestamp, len, "%Y-%m-%d %H:%M:%S %z", tm);
	else
		snprintf(timestamp, len, "unknown");
}

#ifdef HAVE_SQLITE3

#include <sqlite3.h>

static int nvidia_add_vendor_table(struct ras_events *ras,
				   struct ras_ns_ev_decoder *ev_decoder,
				   const struct db_table_descriptor *table,
				   const char *label)
{
	int rc;

	rc = ras_mc_add_vendor_table(ras, &ev_decoder->stmt_dec_record, table);
	if (rc != SQLITE_OK)
		log(TERM, LOG_ERR, "Failed to create/prepare %s table: %d\n",
		    label, rc);

	return rc;
}

static void nvidia_store_vendor_record(struct ras_ns_ev_decoder *ev_decoder,
				       const char *label)
{
	int rc;

	rc = sqlite3_step(ev_decoder->stmt_dec_record);
	if (rc != SQLITE_DONE)
		log(TERM, LOG_ERR,
		    "Failed to store %s event in database: error = %d\n",
		    label, rc);

	rc = sqlite3_reset(ev_decoder->stmt_dec_record);
	if (rc != SQLITE_OK)
		log(TERM, LOG_ERR,
		    "Failed to reset %s statement: error = %d\n",
		    label, rc);
}

#endif  /* HAVE_SQLITE3 */

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

#define NVIDIA_VERA_MAX_CONTEXTS 16
#define NVIDIA_VERA_VERSION 1

static uint16_t nvidia_vera_le16(const void *p)
{
	uint16_t v;

	memcpy(&v, p, sizeof(v));
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
	v = __builtin_bswap16(v);
#endif
	return v;
}

static uint32_t nvidia_vera_le32(const void *p)
{
	uint32_t v;

	memcpy(&v, p, sizeof(v));
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
	v = __builtin_bswap32(v);
#endif
	return v;
}

static uint64_t nvidia_vera_le64(const void *p)
{
	uint64_t v;

	memcpy(&v, p, sizeof(v));
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
	v = __builtin_bswap64(v);
#endif
	return v;
}

struct nvidia_vera_event_sec {
	uint8_t version;
	uint8_t event_context_count;
	uint8_t source_device_type;
	uint8_t reserved;
	uint16_t event_type;
	uint16_t event_sub_type;
	uint64_t event_link_id;
	char source_module_signature[16];
} __attribute__((packed));

struct nvidia_vera_cpu_info {
	uint16_t info_version;
	uint8_t info_size;
	uint8_t socket_number;
	uint32_t architecture;
	uint8_t chip_serial_number[16];
	uint64_t instance_base;
} __attribute__((packed));

struct nvidia_vera_context_sec {
	uint32_t context_size;
	uint16_t context_version;
	uint16_t reserved;
	uint16_t data_format_type;
	uint16_t data_format_version;
	uint32_t data_size;
} __attribute__((packed));

struct nvidia_vera_context {
	uint32_t context_size;
	uint16_t context_version;
	uint16_t data_format_type;
	uint16_t data_format_version;
	uint32_t data_size;
	const uint8_t *data;
};

struct nvidia_vera_decoded {
	char signature[17];
	uint8_t version;
	uint8_t event_context_count;
	uint8_t source_device_type;
	uint16_t event_type;
	uint16_t event_sub_type;
	uint64_t event_link_id;
	uint8_t socket;
	uint32_t architecture;
	uint8_t chip_serial_number[16];
	uint64_t instance_base;
	struct nvidia_vera_context contexts[NVIDIA_VERA_MAX_CONTEXTS];
};

static void nvidia_trace_hex_bytes(struct trace_seq *s, const uint8_t *buf, size_t len)
{
	size_t i;

	for (i = 0; i < len; i++)
		trace_seq_printf(s, "%02x", buf[i]);
}

static int nvidia_vera_context_entry_count(const struct nvidia_vera_context *ctx)
{
	if (!ctx)
		return -EINVAL;
	if (ctx->data_size > INT_MAX)
		return -EOVERFLOW;

	switch (ctx->data_format_type) {
	case 0:
		return 0;
	case 1:
		return ctx->data_size / 16;
	case 2:
	case 3:
		return ctx->data_size / 8;
	case 4:
		return ctx->data_size / 4;
	default:
		return -EINVAL;
	}
}

static int nvidia_vera_context_u64_pair(const struct nvidia_vera_context *ctx,
					unsigned int index, uint64_t *addr, uint64_t *val)
{
	int count;

	if (!ctx || !addr || !val || ctx->data_format_type != 1)
		return -EINVAL;

	count = nvidia_vera_context_entry_count(ctx);
	if (count < 0)
		return count;
	if (index >= (unsigned int)count)
		return -ERANGE;

	*addr = nvidia_vera_le64(ctx->data + index * 16);
	*val = nvidia_vera_le64(ctx->data + index * 16 + 8);

	return 0;
}

static int nvidia_vera_context_u32_pair(const struct nvidia_vera_context *ctx,
					unsigned int index, uint32_t *addr, uint32_t *val)
{
	int count;

	if (!ctx || !addr || !val || ctx->data_format_type != 2)
		return -EINVAL;

	count = nvidia_vera_context_entry_count(ctx);
	if (count < 0)
		return count;
	if (index >= (unsigned int)count)
		return -ERANGE;

	*addr = nvidia_vera_le32(ctx->data + index * 8);
	*val = nvidia_vera_le32(ctx->data + index * 8 + 4);

	return 0;
}

static int nvidia_vera_context_u64_value(const struct nvidia_vera_context *ctx,
					 unsigned int index, uint64_t *val)
{
	int count;

	if (!ctx || !val || ctx->data_format_type != 3)
		return -EINVAL;

	count = nvidia_vera_context_entry_count(ctx);
	if (count < 0)
		return count;
	if (index >= (unsigned int)count)
		return -ERANGE;

	*val = nvidia_vera_le64(ctx->data + index * 8);

	return 0;
}

static int nvidia_vera_context_u32_value(const struct nvidia_vera_context *ctx,
					 unsigned int index, uint32_t *val)
{
	int count;

	if (!ctx || !val || ctx->data_format_type != 4)
		return -EINVAL;

	count = nvidia_vera_context_entry_count(ctx);
	if (count < 0)
		return count;
	if (index >= (unsigned int)count)
		return -ERANGE;

	*val = nvidia_vera_le32(ctx->data + index * 4);

	return 0;
}

static int nvidia_vera_validate_context_data(uint16_t data_format_type,
					     uint32_t data_size)
{
	switch (data_format_type) {
	case 0:
		return 0;
	case 1:
		return data_size % 16 ? -EINVAL : 0;
	case 2:
	case 3:
		return data_size % 8 ? -EINVAL : 0;
	case 4:
		return data_size % 4 ? -EINVAL : 0;
	default:
		return -EOPNOTSUPP;
	}
}

static void nvidia_vera_print_context(struct trace_seq *s,
				      const struct nvidia_vera_context *ctx,
				      unsigned int index)
{
	int entries;
	unsigned int i;
	uint64_t addr64, val64;
	uint32_t addr32, val32;
	int rc;

	trace_seq_printf(s,
			 "context[%u]: version=%u format=%u format_version=%u context_size=%u data_size=%u\n",
			 index, ctx->context_version, ctx->data_format_type,
			 ctx->data_format_version, ctx->context_size,
			 ctx->data_size);

	if (ctx->data_format_type == 0 && ctx->data_size > 0) {
		unsigned int prefix_len = ctx->data_size > 16 ? 16 : ctx->data_size;

		trace_seq_printf(s, "context[%u]_opaque_prefix: ", index);
		nvidia_trace_hex_bytes(s, ctx->data, prefix_len);
		trace_seq_printf(s, "\n");
		return;
	}

	entries = nvidia_vera_context_entry_count(ctx);
	if (entries < 0) {
		trace_seq_printf(s, "  WARNING: invalid context data (%d)\n", entries);
		return;
	}

	trace_seq_printf(s, "context[%u]_entries: %d\n", index, entries);

	for (i = 0; i < (unsigned int)entries; i++) {
		switch (ctx->data_format_type) {
		case 1:
			rc = nvidia_vera_context_u64_pair(ctx, i, &addr64, &val64);
			if (!rc)
				trace_seq_printf(s, "  context[%u].reg[%u]: addr=0x%016llx val=0x%016llx\n",
						 index, i,
						 (unsigned long long)addr64,
						 (unsigned long long)val64);
			break;
		case 2:
			rc = nvidia_vera_context_u32_pair(ctx, i, &addr32, &val32);
			if (!rc)
				trace_seq_printf(s, "  context[%u].reg[%u]: addr=0x%08x val=0x%08x\n",
						 index, i, addr32, val32);
			break;
		case 3:
			rc = nvidia_vera_context_u64_value(ctx, i, &val64);
			if (!rc)
				trace_seq_printf(s, "  context[%u].value[%u]: 0x%016llx\n",
						 index, i, (unsigned long long)val64);
			break;
		case 4:
			rc = nvidia_vera_context_u32_value(ctx, i, &val32);
			if (!rc)
				trace_seq_printf(s, "  context[%u].value[%u]: 0x%08x\n",
						 index, i, val32);
			break;
		default:
			break;
		}
	}
}

static int nvidia_decode_vera_cper_sec(const void *buf, size_t len,
				       struct nvidia_vera_decoded *decoded)
{
	const struct nvidia_vera_event_sec *event = buf;
	const struct nvidia_vera_cpu_info *cpu_info;
	const struct nvidia_vera_context_sec *context;
	const uint8_t *bytes = buf;
	size_t data_end_advance;
	size_t advance;
	size_t offset;
	int i;
	int ret;

	if (!buf || !decoded)
		return -EINVAL;
	if (len < sizeof(*event))
		return -ENODATA;
	if (event->version != NVIDIA_VERA_VERSION)
		return -EOPNOTSUPP;
	if (event->source_device_type != 0)
		return -EOPNOTSUPP;

	offset = sizeof(*event);
	if (len - offset < sizeof(*cpu_info))
		return -ENODATA;

	cpu_info = (const void *)(bytes + offset);
	if (cpu_info->info_size < sizeof(*cpu_info))
		return -EINVAL;
	if (len - offset < cpu_info->info_size)
		return -ENODATA;

	offset += cpu_info->info_size;
	if (event->event_context_count > NVIDIA_VERA_MAX_CONTEXTS)
		return -E2BIG;

	memset(decoded, 0, sizeof(*decoded));
	memcpy(decoded->signature, event->source_module_signature,
	       sizeof(event->source_module_signature));
	decoded->signature[sizeof(event->source_module_signature)] = '\0';
	decoded->version = event->version;
	decoded->event_context_count = event->event_context_count;
	decoded->source_device_type = event->source_device_type;
	decoded->event_type = nvidia_vera_le16(&event->event_type);
	decoded->event_sub_type = nvidia_vera_le16(&event->event_sub_type);
	decoded->event_link_id = nvidia_vera_le64(&event->event_link_id);
	decoded->socket = cpu_info->socket_number;
	decoded->architecture = nvidia_vera_le32(&cpu_info->architecture);
	memcpy(decoded->chip_serial_number, cpu_info->chip_serial_number,
	       sizeof(cpu_info->chip_serial_number));
	decoded->instance_base = nvidia_vera_le64(&cpu_info->instance_base);

	for (i = 0; i < event->event_context_count; i++) {
		struct nvidia_vera_context *decoded_context = &decoded->contexts[i];
		uint32_t context_size;
		uint32_t data_size;
		uint16_t data_format_type;

		if (len - offset < sizeof(*context))
			return -ENODATA;

		context = (const void *)(bytes + offset);
		context_size = nvidia_vera_le32(&context->context_size);
		data_format_type = nvidia_vera_le16(&context->data_format_type);
		data_size = nvidia_vera_le32(&context->data_size);

		if (context_size < sizeof(*context))
			return -EINVAL;
		if (data_format_type > 4)
			return -EOPNOTSUPP;
		if (data_size > SIZE_MAX - sizeof(*context))
			return -EOVERFLOW;

		data_end_advance = sizeof(*context) + data_size;
		if (data_end_advance > len - offset)
			return -ENODATA;

		if (context_size == sizeof(*context))
			advance = data_end_advance;
		else if (data_size <= context_size - sizeof(*context))
			advance = context_size;
		else
			return -EINVAL;

		if (advance > len - offset)
			return -ENODATA;

		ret = nvidia_vera_validate_context_data(data_format_type, data_size);
		if (ret)
			return ret;

		decoded_context->context_size = context_size;
		decoded_context->context_version =
			nvidia_vera_le16(&context->context_version);
		decoded_context->data_format_type = data_format_type;
		decoded_context->data_format_version =
			nvidia_vera_le16(&context->data_format_version);
		decoded_context->data_size = data_size;
		decoded_context->data = bytes + offset + sizeof(*context);
		offset += advance;
	}

	return 0;
}

#ifdef HAVE_SQLITE3

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
	return nvidia_add_vendor_table(ras, ev_decoder, &nvidia_ns_table,
				       "NVIDIA");
}

static int nvidia_ns_decode(struct ras_events *ras,
			    struct ras_ns_ev_decoder *ev_decoder,
			    struct trace_seq *s,
			    struct ras_non_standard_event *event)
{
	const struct nvidia_cper_sec *err;
	char timestamp[64];
	uint32_t reg_data_len;

	if (event->length < sizeof(struct nvidia_cper_sec)) {
		trace_seq_printf(s, "NVIDIA CPER section too small: %u bytes\n",
				 event->length);
		return -1;
	}

	err = (const struct nvidia_cper_sec *)event->error;

	nvidia_format_timestamp(timestamp, sizeof(timestamp));

	decode_nvidia_cper_sec(ev_decoder, s, err, event->length);

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

		if (reg_data_len > 0) {
			const uint8_t *reg_data = (const uint8_t *)err + sizeof(struct nvidia_cper_sec);

			sqlite3_bind_blob(ev_decoder->stmt_dec_record, 9,
					  reg_data, reg_data_len, SQLITE_TRANSIENT);
		} else {
			sqlite3_bind_null(ev_decoder->stmt_dec_record, 9);
		}

		sqlite3_bind_blob(ev_decoder->stmt_dec_record, 10,
				  event->error, event->length, SQLITE_TRANSIENT);

		nvidia_store_vendor_record(ev_decoder, "NVIDIA");
	}

	return 0;
}

static const struct db_fields nvidia_vera_ns_fields[] = {
	{ .name = "id",				.type = "INTEGER PRIMARY KEY" },
	{ .name = "timestamp",			.type = "TEXT" },
	{ .name = "signature",			.type = "TEXT" },
	{ .name = "event_type",			.type = "INTEGER" },
	{ .name = "event_sub_type",		.type = "INTEGER" },
	{ .name = "event_link_id",		.type = "INTEGER" },
	{ .name = "source_device_type",		.type = "INTEGER" },
	{ .name = "event_context_count",	.type = "INTEGER" },
	{ .name = "socket",			.type = "INTEGER" },
	{ .name = "architecture",		.type = "INTEGER" },
	{ .name = "chip_serial_number",		.type = "BLOB" },
	{ .name = "instance_base",		.type = "INTEGER" },
	{ .name = "raw_data",			.type = "BLOB" },
};

static const struct db_table_descriptor nvidia_vera_ns_table = {
	.name = "nvidia_vera_ns_event",
	.fields = nvidia_vera_ns_fields,
	.num_fields = ARRAY_SIZE(nvidia_vera_ns_fields),
};

static int nvidia_vera_ns_add_table(struct ras_events *ras,
				    struct ras_ns_ev_decoder *ev_decoder)
{
	return nvidia_add_vendor_table(ras, ev_decoder, &nvidia_vera_ns_table,
				       "NVIDIA Vera");
}

static int nvidia_vera_ns_decode(struct ras_events *ras,
				 struct ras_ns_ev_decoder *ev_decoder,
				 struct trace_seq *s,
				 struct ras_non_standard_event *event)
{
	struct nvidia_vera_decoded decoded;
	char timestamp[64];
	int rc;
	unsigned int i;

	if (event->length < sizeof(struct nvidia_vera_event_sec) +
			   sizeof(struct nvidia_vera_cpu_info)) {
		trace_seq_printf(s, "NVIDIA Vera CPER section too small: %u bytes\n",
				 event->length);
		return -1;
	}

	rc = nvidia_decode_vera_cper_sec(event->error, event->length, &decoded);
	if (rc) {
		trace_seq_printf(s, "Malformed NVIDIA Vera CPER section, error_data_length: %u, ret: %d\n",
				 event->length, rc);
		return -1;
	}

	nvidia_format_timestamp(timestamp, sizeof(timestamp));

	trace_seq_printf(s, "NVIDIA Vera CPER section, error_data_length: %u\n",
			 event->length);
	trace_seq_printf(s, "version: %u\n", decoded.version);
	trace_seq_printf(s, "signature: %s\n", decoded.signature);
	trace_seq_printf(s, "event_type: %u\n", decoded.event_type);
	trace_seq_printf(s, "event_sub_type: %u\n", decoded.event_sub_type);
	trace_seq_printf(s, "event_link_id: 0x%016llx\n",
			 (unsigned long long)decoded.event_link_id);
	trace_seq_printf(s, "source_device_type: %u\n", decoded.source_device_type);
	trace_seq_printf(s, "socket: %u\n", decoded.socket);
	trace_seq_printf(s, "architecture: 0x%x\n", decoded.architecture);
	trace_seq_printf(s, "chip_serial_number: ");
	nvidia_trace_hex_bytes(s, decoded.chip_serial_number,
			       sizeof(decoded.chip_serial_number));
	trace_seq_printf(s, "\n");
	trace_seq_printf(s, "instance_base: 0x%016llx\n",
			 (unsigned long long)decoded.instance_base);
	trace_seq_printf(s, "event_context_count: %u\n", decoded.event_context_count);

	for (i = 0; i < decoded.event_context_count; i++)
		nvidia_vera_print_context(s, &decoded.contexts[i], i);

	if (ev_decoder->stmt_dec_record) {
		sqlite3_bind_text(ev_decoder->stmt_dec_record, 1, timestamp, -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(ev_decoder->stmt_dec_record, 2, decoded.signature,
				  sizeof(decoded.signature) - 1, SQLITE_TRANSIENT);
		sqlite3_bind_int(ev_decoder->stmt_dec_record, 3, decoded.event_type);
		sqlite3_bind_int(ev_decoder->stmt_dec_record, 4, decoded.event_sub_type);
		sqlite3_bind_int64(ev_decoder->stmt_dec_record, 5, decoded.event_link_id);
		sqlite3_bind_int(ev_decoder->stmt_dec_record, 6, decoded.source_device_type);
		sqlite3_bind_int(ev_decoder->stmt_dec_record, 7, decoded.event_context_count);
		sqlite3_bind_int(ev_decoder->stmt_dec_record, 8, decoded.socket);
		sqlite3_bind_int64(ev_decoder->stmt_dec_record, 9, decoded.architecture);
		sqlite3_bind_blob(ev_decoder->stmt_dec_record, 10,
				  decoded.chip_serial_number,
				  sizeof(decoded.chip_serial_number), SQLITE_TRANSIENT);
		sqlite3_bind_int64(ev_decoder->stmt_dec_record, 11, decoded.instance_base);
		sqlite3_bind_blob(ev_decoder->stmt_dec_record, 12,
				  event->error, event->length, SQLITE_TRANSIENT);

		nvidia_store_vendor_record(ev_decoder, "NVIDIA Vera");
	}

	return 0;
}

#endif  /* HAVE_SQLITE3 */

struct ras_ns_ev_decoder nvidia_ns_ev_decoder = {
	.sec_type = NVIDIA_GRACE_SEC_TYPE_UUID,
#ifdef HAVE_SQLITE3
	.add_table = nvidia_ns_add_table,
	.decode = nvidia_ns_decode,
#endif
};

static struct ras_ns_ev_decoder nvidia_vera_ns_ev_decoder = {
	.sec_type = NVIDIA_VERA_SEC_TYPE_UUID,
#ifdef HAVE_SQLITE3
	.add_table = nvidia_vera_ns_add_table,
	.decode = nvidia_vera_ns_decode,
#endif
};

static void __attribute__((constructor)) nvidia_init(void)
{
	register_ns_ev_decoder(&nvidia_ns_ev_decoder);
	register_ns_ev_decoder(&nvidia_vera_ns_ev_decoder);
}
