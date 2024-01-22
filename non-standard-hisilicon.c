/*
 * Copyright (c) 2020 Hisilicon Limited.
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
#include "ras-record.h"
#include "ras-logger.h"
#include "ras-report.h"
#include "non-standard-hisilicon.h"

#define HISI_BUF_LEN	2048
#define HISI_PCIE_INFO_BUF_LEN	256

struct hisi_common_error_section {
	uint32_t   val_bits;
	uint8_t    version;
	uint8_t    soc_id;
	uint8_t    socket_id;
	uint8_t    totem_id;
	uint8_t    nimbus_id;
	uint8_t    subsystem_id;
	uint8_t    module_id;
	uint8_t    submodule_id;
	uint8_t    core_id;
	uint8_t    port_id;
	uint16_t   err_type;
	struct {
		uint8_t  function;
		uint8_t  device;
		uint16_t segment;
		uint8_t  bus;
		uint8_t  reserved[3];
	}          pcie_info;
	uint8_t    err_severity;
	uint8_t    reserved[3];
	uint32_t   reg_array_size;
	uint32_t   reg_array[];
};

enum {
	HISI_COMMON_VALID_SOC_ID,
	HISI_COMMON_VALID_SOCKET_ID,
	HISI_COMMON_VALID_TOTEM_ID,
	HISI_COMMON_VALID_NIMBUS_ID,
	HISI_COMMON_VALID_SUBSYSTEM_ID,
	HISI_COMMON_VALID_MODULE_ID,
	HISI_COMMON_VALID_SUBMODULE_ID,
	HISI_COMMON_VALID_CORE_ID,
	HISI_COMMON_VALID_PORT_ID,
	HISI_COMMON_VALID_ERR_TYPE,
	HISI_COMMON_VALID_PCIE_INFO,
	HISI_COMMON_VALID_ERR_SEVERITY,
	HISI_COMMON_VALID_REG_ARRAY_SIZE,
};

enum {
	HISI_COMMON_FIELD_ID,
	HISI_COMMON_FIELD_TIMESTAMP,
	HISI_COMMON_FIELD_VERSION,
	HISI_COMMON_FIELD_SOC_ID,
	HISI_COMMON_FIELD_SOCKET_ID,
	HISI_COMMON_FIELD_TOTEM_ID,
	HISI_COMMON_FIELD_NIMBUS_ID,
	HISI_COMMON_FIELD_SUB_SYSTEM_ID,
	HISI_COMMON_FIELD_MODULE_ID,
	HISI_COMMON_FIELD_SUB_MODULE_ID,
	HISI_COMMON_FIELD_CORE_ID,
	HISI_COMMON_FIELD_PORT_ID,
	HISI_COMMON_FIELD_ERR_TYPE,
	HISI_COMMON_FIELD_PCIE_INFO,
	HISI_COMMON_FIELD_ERR_SEVERITY,
	HISI_COMMON_FIELD_REGS_DUMP,
};

struct hisi_event {
	char error_msg[HISI_BUF_LEN];
	char pcie_info[HISI_PCIE_INFO_BUF_LEN];
	char reg_msg[HISI_BUF_LEN];
};

#ifdef HAVE_SQLITE3
void record_vendor_data(struct ras_ns_ev_decoder *ev_decoder,
			enum hisi_oem_data_type data_type,
			       int id, int64_t data, const char *text)
{
	if (!ev_decoder->stmt_dec_record)
		return;

	switch (data_type) {
	case HISI_OEM_DATA_TYPE_INT:
		sqlite3_bind_int(ev_decoder->stmt_dec_record, id, data);
		break;
	case HISI_OEM_DATA_TYPE_INT64:
		sqlite3_bind_int64(ev_decoder->stmt_dec_record, id, data);
		break;
	case HISI_OEM_DATA_TYPE_TEXT:
		sqlite3_bind_text(ev_decoder->stmt_dec_record, id, text, -1, NULL);
		break;
	}
}

int step_vendor_data_tab(struct ras_ns_ev_decoder *ev_decoder, const char *name)
{
	int rc;

	if (!ev_decoder->stmt_dec_record)
		return 0;

	rc = sqlite3_step(ev_decoder->stmt_dec_record);
	if (rc != SQLITE_OK && rc != SQLITE_DONE)
		log(TERM, LOG_ERR,
		    "Failed to do %s step on sqlite: error = %d\n", name, rc);

	rc = sqlite3_reset(ev_decoder->stmt_dec_record);
	if (rc != SQLITE_OK && rc != SQLITE_DONE)
		log(TERM, LOG_ERR,
		    "Failed to reset %s on sqlite: error = %d\n", name, rc);

	rc = sqlite3_clear_bindings(ev_decoder->stmt_dec_record);
	if (rc != SQLITE_OK && rc != SQLITE_DONE)
		log(TERM, LOG_ERR,
		    "Failed to clear bindings %s on sqlite: error = %d\n",
		    name, rc);

	return rc;
}
#else
void record_vendor_data(struct ras_ns_ev_decoder *ev_decoder,
			enum hisi_oem_data_type data_type,
			int id, int64_t data, const char *text)
{ }

int step_vendor_data_tab(struct ras_ns_ev_decoder *ev_decoder, const char *name)
{
	return 0;
}
#endif

#ifdef HAVE_SQLITE3
static const struct db_fields hisi_common_section_fields[] = {
	{ .name = "id",			.type = "INTEGER PRIMARY KEY" },
	{ .name = "timestamp",		.type = "TEXT" },
	{ .name = "version",		.type = "INTEGER" },
	{ .name = "soc_id",		.type = "INTEGER" },
	{ .name = "socket_id",		.type = "INTEGER" },
	{ .name = "totem_id",		.type = "INTEGER" },
	{ .name = "nimbus_id",		.type = "INTEGER" },
	{ .name = "sub_system_id",	.type = "INTEGER" },
	{ .name = "module_id",		.type = "TEXT" },
	{ .name = "sub_module_id",	.type = "INTEGER" },
	{ .name = "core_id",		.type = "INTEGER" },
	{ .name = "port_id",		.type = "INTEGER" },
	{ .name = "err_type",		.type = "INTEGER" },
	{ .name = "pcie_info",		.type = "TEXT" },
	{ .name = "err_severity",	.type = "TEXT" },
	{ .name = "regs_dump",		.type = "TEXT" },
};

static const struct db_table_descriptor hisi_common_section_tab = {
	.name = "hisi_common_section_v2",
	.fields = hisi_common_section_fields,
	.num_fields = ARRAY_SIZE(hisi_common_section_fields),
};
#endif

static const char *soc_desc[] = {
	"Kunpeng916",
	"Kunpeng920",
	"Kunpeng930",
};

static const char *module_name[] = {
	"MN",
	"PLL",
	"SLLC",
	"AA",
	"SIOE",
	"POE",
	"CPA",
	"DISP",
	"GIC",
	"ITS",
	"AVSBUS",
	"CS",
	"PPU",
	"SMMU",
	"PA",
	"HLLC",
	"DDRC",
	"L3TAG",
	"L3DATA",
	"PCS",
	"MATA",
	"PCIe Local",
	"SAS",
	"SATA",
	"NIC",
	"RoCE",
	"USB",
	"ZIP",
	"HPRE",
	"SEC",
	"RDE",
	"MEE",
	"L4D",
	"Tsensor",
	"ROH",
	"BTC",
	"HILINK",
	"STARS",
	"SDMA",
	"UC",
	"HBMC",
};

static const char *get_soc_desc(uint8_t soc_id)
{
	if (soc_id >= sizeof(soc_desc) / sizeof(char *))
		return "unknown";

	return soc_desc[soc_id];
}

static void decode_module(struct ras_ns_ev_decoder *ev_decoder,
			  struct hisi_event *event, uint8_t module_id)
{
	if (module_id >= sizeof(module_name) / sizeof(char *)) {
		HISI_SNPRINTF(event->error_msg, "module=unknown(id=%hhu) ", module_id);
		record_vendor_data(ev_decoder, HISI_OEM_DATA_TYPE_TEXT,
				   HISI_COMMON_FIELD_MODULE_ID,
				   0, "unknown");
	} else {
		HISI_SNPRINTF(event->error_msg, "module=%s ", module_name[module_id]);
		record_vendor_data(ev_decoder, HISI_OEM_DATA_TYPE_TEXT,
				   HISI_COMMON_FIELD_MODULE_ID,
				   0, module_name[module_id]);
	}
}

static void decode_hisi_common_section_hdr(struct ras_ns_ev_decoder *ev_decoder,
					   const struct hisi_common_error_section *err,
					  struct hisi_event *event)
{
	HISI_SNPRINTF(event->error_msg, "[ table_version=%hhu", err->version);
	record_vendor_data(ev_decoder, HISI_OEM_DATA_TYPE_INT,
			   HISI_COMMON_FIELD_VERSION,
			   err->version, NULL);
	if (err->val_bits & BIT(HISI_COMMON_VALID_SOC_ID)) {
		HISI_SNPRINTF(event->error_msg, "soc=%s", get_soc_desc(err->soc_id));
		record_vendor_data(ev_decoder, HISI_OEM_DATA_TYPE_INT,
				   HISI_COMMON_FIELD_SOC_ID,
				   err->soc_id, NULL);
	}

	if (err->val_bits & BIT(HISI_COMMON_VALID_SOCKET_ID)) {
		HISI_SNPRINTF(event->error_msg, "socket_id=%hhu", err->socket_id);
		record_vendor_data(ev_decoder, HISI_OEM_DATA_TYPE_INT,
				   HISI_COMMON_FIELD_SOCKET_ID,
				   err->socket_id, NULL);
	}

	if (err->val_bits & BIT(HISI_COMMON_VALID_TOTEM_ID)) {
		HISI_SNPRINTF(event->error_msg, "totem_id=%hhu", err->totem_id);
		record_vendor_data(ev_decoder, HISI_OEM_DATA_TYPE_INT,
				   HISI_COMMON_FIELD_TOTEM_ID,
				   err->totem_id, NULL);
	}

	if (err->val_bits & BIT(HISI_COMMON_VALID_NIMBUS_ID)) {
		HISI_SNPRINTF(event->error_msg, "nimbus_id=%hhu", err->nimbus_id);
		record_vendor_data(ev_decoder, HISI_OEM_DATA_TYPE_INT,
				   HISI_COMMON_FIELD_NIMBUS_ID,
				   err->nimbus_id, NULL);
	}

	if (err->val_bits & BIT(HISI_COMMON_VALID_SUBSYSTEM_ID)) {
		HISI_SNPRINTF(event->error_msg, "subsystem_id=%hhu", err->subsystem_id);
		record_vendor_data(ev_decoder, HISI_OEM_DATA_TYPE_INT,
				   HISI_COMMON_FIELD_SUB_SYSTEM_ID,
				   err->subsystem_id, NULL);
	}

	if (err->val_bits & BIT(HISI_COMMON_VALID_MODULE_ID))
		decode_module(ev_decoder, event, err->module_id);

	if (err->val_bits & BIT(HISI_COMMON_VALID_SUBMODULE_ID)) {
		HISI_SNPRINTF(event->error_msg, "submodule_id=%hhu", err->submodule_id);
		record_vendor_data(ev_decoder, HISI_OEM_DATA_TYPE_INT,
				   HISI_COMMON_FIELD_SUB_MODULE_ID,
				   err->submodule_id, NULL);
	}

	if (err->val_bits & BIT(HISI_COMMON_VALID_CORE_ID)) {
		HISI_SNPRINTF(event->error_msg, "core_id=%hhu", err->core_id);
		record_vendor_data(ev_decoder, HISI_OEM_DATA_TYPE_INT,
				   HISI_COMMON_FIELD_CORE_ID,
				   err->core_id, NULL);
	}

	if (err->val_bits & BIT(HISI_COMMON_VALID_PORT_ID)) {
		HISI_SNPRINTF(event->error_msg, "port_id=%hhu", err->port_id);
		record_vendor_data(ev_decoder, HISI_OEM_DATA_TYPE_INT,
				   HISI_COMMON_FIELD_PORT_ID,
				   err->port_id, NULL);
	}

	if (err->val_bits & BIT(HISI_COMMON_VALID_ERR_TYPE)) {
		HISI_SNPRINTF(event->error_msg, "err_type=%hu", err->err_type);
		record_vendor_data(ev_decoder, HISI_OEM_DATA_TYPE_INT,
				   HISI_COMMON_FIELD_ERR_TYPE,
				   err->err_type, NULL);
	}

	if (err->val_bits & BIT(HISI_COMMON_VALID_PCIE_INFO)) {
		HISI_SNPRINTF(event->error_msg, "pcie_device_id=%04x:%02x:%02x.%x",
			      err->pcie_info.segment, err->pcie_info.bus,
			      err->pcie_info.device, err->pcie_info.function);
		HISI_SNPRINTF(event->pcie_info, "%04x:%02x:%02x.%x",
			      err->pcie_info.segment, err->pcie_info.bus,
			      err->pcie_info.device, err->pcie_info.function);
		record_vendor_data(ev_decoder, HISI_OEM_DATA_TYPE_TEXT,
				   HISI_COMMON_FIELD_PCIE_INFO,
				   0, event->pcie_info);
	}

	if (err->val_bits & BIT(HISI_COMMON_VALID_ERR_SEVERITY)) {
		HISI_SNPRINTF(event->error_msg, "err_severity=%s", err_severity(err->err_severity));
		record_vendor_data(ev_decoder, HISI_OEM_DATA_TYPE_TEXT,
				   HISI_COMMON_FIELD_ERR_SEVERITY,
				   0, err_severity(err->err_severity));
	}

	HISI_SNPRINTF(event->error_msg, "]");
}

static int add_hisi_common_table(struct ras_events *ras,
				 struct ras_ns_ev_decoder *ev_decoder)
{
#ifdef HAVE_SQLITE3
	if (ras->record_events &&
	    !ev_decoder->stmt_dec_record) {
		if (ras_mc_add_vendor_table(ras, &ev_decoder->stmt_dec_record,
					    &hisi_common_section_tab) != SQLITE_OK) {
			log(TERM, LOG_WARNING, "Failed to create sql hisi_common_section_tab\n");
			return -1;
		}
	}
#endif

	return 0;
}

static int decode_hisi_common_section(struct ras_events *ras,
				      struct ras_ns_ev_decoder *ev_decoder,
				      struct trace_seq *s,
				      struct ras_non_standard_event *event)
{
	const struct hisi_common_error_section *err =
			(struct hisi_common_error_section *)event->error;
	struct hisi_event hevent;

	memset(&hevent, 0, sizeof(struct hisi_event));
	trace_seq_printf(s, "\nHisilicon Common Error Section:\n");
	decode_hisi_common_section_hdr(ev_decoder, err, &hevent);
	trace_seq_printf(s, "%s\n", hevent.error_msg);

	if (err->val_bits & BIT(HISI_COMMON_VALID_REG_ARRAY_SIZE) && err->reg_array_size > 0) {
		unsigned int i;

		trace_seq_printf(s, "Register Dump:\n");
		for (i = 0; i < err->reg_array_size / sizeof(uint32_t); i++) {
			trace_seq_printf(s, "reg%02u=0x%08x\n", i,
					 err->reg_array[i]);
			HISI_SNPRINTF(hevent.reg_msg, "reg%02u=0x%08x",
				      i, err->reg_array[i]);
		}
	}

	if (ras->record_events) {
		record_vendor_data(ev_decoder, HISI_OEM_DATA_TYPE_TEXT,
				   HISI_COMMON_FIELD_TIMESTAMP,
				   0, event->timestamp);
		record_vendor_data(ev_decoder, HISI_OEM_DATA_TYPE_TEXT,
				   HISI_COMMON_FIELD_REGS_DUMP, 0, hevent.reg_msg);
		step_vendor_data_tab(ev_decoder, "hisi_common_section_tab");
	}

	return 0;
}

static struct ras_ns_ev_decoder hisi_section_ns_ev_decoder[]  = {
	{
		.sec_type = "c8b328a8-9917-4af6-9a13-2e08ab2e7586",
		.add_table = add_hisi_common_table,
		.decode = decode_hisi_common_section,
	},
};

static void __attribute__((constructor)) hisi_ns_init(void)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(hisi_section_ns_ev_decoder); i++)
		register_ns_ev_decoder(&hisi_section_ns_ev_decoder[i]);
}
