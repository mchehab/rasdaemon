// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * Copyright (c) 2020 Hisilicon Limited.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/ras-logger.h"
#include "core/modules.h"
#include "core/types.h"
#include "events-arch-arm/non-standard-hisilicon.h"
#include "modules/ras-report.h"

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

static const struct db_fields hisi_common_section_fields[] = {
	{ .name = "id",			.type = DB_TYPE_SERIAL, .is_pk = true },
	{ .name = "timestamp",		.type = DB_TYPE_TIMESTAMP},
	{ .name = "version",		.type = DB_TYPE_INT32 },
	{ .name = "soc_id",		.type = DB_TYPE_INT32 },
	{ .name = "socket_id",		.type = DB_TYPE_INT32 },
	{ .name = "totem_id",		.type = DB_TYPE_INT32 },
	{ .name = "nimbus_id",		.type = DB_TYPE_INT32 },
	{ .name = "sub_system_id",	.type = DB_TYPE_INT32 },
	{ .name = "module_id",		.type = DB_TYPE_TEXT },
	{ .name = "sub_module_id",	.type = DB_TYPE_INT32 },
	{ .name = "core_id",		.type = DB_TYPE_INT32 },
	{ .name = "port_id",		.type = DB_TYPE_INT32 },
	{ .name = "err_type",		.type = DB_TYPE_INT32 },
	{ .name = "pcie_info",		.type = DB_TYPE_TEXT },
	{ .name = "err_severity",	.type = DB_TYPE_TEXT },
	{ .name = "regs_dump",		.type = DB_TYPE_TEXT },
};

static const struct db_table_descriptor hisi_common_section_tab = {
	.name = "hisi_common_section_v2",
	.fields = hisi_common_section_fields,
	.num_fields = ARRAY_SIZE(hisi_common_section_fields),
};

static struct db_desc_and_stmt hisi_common_section_db = {
	.desc = &hisi_common_section_tab,
};

static const char * const soc_desc[] = {
	"Kunpeng916",
	"Kunpeng920",
	"Kunpeng930",
};

static const char * const module_name[] = {
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
	"HHA",
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
	"PMC",
	"SCHE",
	"ASMB_DFS",
	"ASMB_NTU",
	"UB",
	"UMMU",
	"PCU",
	"UCMI",
	"DJTAGM",
	"CFGBUS",
	"MPU",
	"CRG",
	"ACG3",
	"DCIP",
	"UMAU",
	"UPA",
	"AXI_MSTR_OOO",
	"RBIST",
	"LC300",
};

static const char * const get_soc_desc(uint8_t soc_id)
{
	if (soc_id >= sizeof(soc_desc) / sizeof(char *))
		return "unknown";

	return soc_desc[soc_id];
}

static void decode_module(struct ras_stmt *stmt, struct hisi_event *event,
			  uint8_t module_id)
{
	if (module_id >= sizeof(module_name) / sizeof(char *)) {
		HISI_SNPRINTF(event->error_msg, "module=unknown(id=%hhu) ",
			      module_id);
		db_bind(&hisi_common_section_tab, stmt,
			HISI_COMMON_FIELD_MODULE_ID,
			(uint64_t)"unknown", -1);
	} else {
		HISI_SNPRINTF(event->error_msg, "module=%s ",
			      module_name[module_id]);
		db_bind(&hisi_common_section_tab, stmt,
			HISI_COMMON_FIELD_MODULE_ID,
			(uint64_t)module_name[module_id], -1);
	}
}

static void decode_int_fields(struct ras_stmt *stmt, int id, uint16_t data,
			      struct hisi_event *event, bool valid)
{
	if (!valid)
		return;

	if (id == HISI_COMMON_FIELD_SOC_ID) {
		HISI_SNPRINTF(event->error_msg, "soc=%s", get_soc_desc(data));
	} else {
		HISI_SNPRINTF(event->error_msg, "%s=%hu",
			      hisi_common_section_fields[id].name, data);
	}

	db_bind(&hisi_common_section_tab, stmt, id, data, -1);
}

static void decode_text_fields(struct ras_stmt *stmt, int id,
			       const struct hisi_common_error_section *err,
			       struct hisi_event *event, bool valid)
{
	if (!valid)
		return;

	if (id == HISI_COMMON_FIELD_MODULE_ID)
		decode_module(stmt, event, err->module_id);

	if (id == HISI_COMMON_FIELD_PCIE_INFO) {
		HISI_SNPRINTF(event->error_msg,
			      "pcie_device_id=%04x:%02x:%02x.%x",
			      err->pcie_info.segment, err->pcie_info.bus,
			      err->pcie_info.device, err->pcie_info.function);
		HISI_SNPRINTF(event->pcie_info, "%04x:%02x:%02x.%x",
			      err->pcie_info.segment, err->pcie_info.bus,
			      err->pcie_info.device, err->pcie_info.function);
		db_bind(&hisi_common_section_tab, stmt, id,
			(uint64_t)event->pcie_info, -1);
	}

	if (id == HISI_COMMON_FIELD_ERR_SEVERITY) {
		HISI_SNPRINTF(event->error_msg, "err_severity=%s",
			      err_severity(err->err_severity));
		db_bind(&hisi_common_section_tab, stmt, id,
			(uint64_t)err_severity(err->err_severity), -1);
	}
}

static void
decode_hisi_common_section_hdr(struct ras_stmt *stmt,
			       const struct hisi_common_error_section *err,
			       struct hisi_event *event)
{
	HISI_SNPRINTF(event->error_msg, "[");

	decode_int_fields(stmt, HISI_COMMON_FIELD_VERSION, err->version, event,
			  1);
	decode_int_fields(stmt, HISI_COMMON_FIELD_SOC_ID, err->soc_id, event,
			  err->val_bits & BIT(HISI_COMMON_VALID_SOC_ID));
	decode_int_fields(stmt, HISI_COMMON_FIELD_SOCKET_ID, err->socket_id,
			  event,
			  err->val_bits & BIT(HISI_COMMON_VALID_SOCKET_ID));
	decode_int_fields(stmt, HISI_COMMON_FIELD_TOTEM_ID, err->totem_id,
			  event,
			  err->val_bits & BIT(HISI_COMMON_VALID_TOTEM_ID));
	decode_int_fields(stmt, HISI_COMMON_FIELD_NIMBUS_ID, err->nimbus_id,
			  event,
			  err->val_bits & BIT(HISI_COMMON_VALID_NIMBUS_ID));
	decode_int_fields(stmt, HISI_COMMON_FIELD_SUB_SYSTEM_ID,
			  err->subsystem_id, event,
			  err->val_bits & BIT(HISI_COMMON_VALID_SUBSYSTEM_ID));

	decode_text_fields(stmt, HISI_COMMON_FIELD_MODULE_ID, err, event,
			   err->val_bits & BIT(HISI_COMMON_VALID_MODULE_ID));

	decode_int_fields(stmt, HISI_COMMON_FIELD_SUB_MODULE_ID,
			  err->submodule_id, event,
			  err->val_bits & BIT(HISI_COMMON_VALID_SUBMODULE_ID));
	decode_int_fields(stmt, HISI_COMMON_FIELD_CORE_ID, err->core_id, event,
			  err->val_bits & BIT(HISI_COMMON_VALID_CORE_ID));
	decode_int_fields(stmt, HISI_COMMON_FIELD_PORT_ID, err->port_id, event,
			  err->val_bits & BIT(HISI_COMMON_VALID_PORT_ID));
	decode_int_fields(stmt, HISI_COMMON_FIELD_ERR_TYPE, err->err_type,
			  event,
			  err->val_bits & BIT(HISI_COMMON_VALID_ERR_TYPE));

	decode_text_fields(stmt, HISI_COMMON_FIELD_PCIE_INFO, err, event,
			   err->val_bits & BIT(HISI_COMMON_VALID_PCIE_INFO));
	decode_text_fields(stmt, HISI_COMMON_FIELD_ERR_SEVERITY, err, event,
			   err->val_bits & BIT(HISI_COMMON_VALID_ERR_SEVERITY));

	HISI_SNPRINTF(event->error_msg, "]");
}

static int decode_hisi_common_section(struct ras_events *ras,
				      struct ras_ns_ev_decoder *ev_decoder,
				      struct trace_seq *s,
				      struct ras_non_standard_event *event)
{
	const struct hisi_common_error_section *err =
	    (struct hisi_common_error_section *)event->error;
	struct ras_stmt *stmt = hisi_common_section_db.stmt;
	struct hisi_event hevent;

	if (ras->record_events)
		WARN_ONCE(!stmt, ALL, LOG_WARNING,
			  "Can't insert into table %s: no statement\n",
			  hisi_common_section_db.desc->name);

	memset(&hevent, 0, sizeof(struct hisi_event));
	trace_seq_printf(s, "\nHisilicon Common Error Section:\n");
	decode_hisi_common_section_hdr(stmt, err, &hevent);
	trace_seq_printf(s, "%s\n", hevent.error_msg);

	if (err->val_bits & BIT(HISI_COMMON_VALID_REG_ARRAY_SIZE) &&
	    err->reg_array_size > 0) {
		unsigned int i;

		trace_seq_printf(s, "Register Dump:\n");
		for (i = 0; i < err->reg_array_size / sizeof(uint32_t); i++) {
			trace_seq_printf(s, "reg%02u=0x%08x\n", i,
					 err->reg_array[i]);
			HISI_SNPRINTF(hevent.reg_msg, "reg%02u=0x%08x", i,
				      err->reg_array[i]);
		}
	}

	if (ras->record_events) {
		db_bind(&hisi_common_section_tab, stmt,
			HISI_COMMON_FIELD_TIMESTAMP,
			(uint64_t)event->timestamp, -1);
		db_bind(&hisi_common_section_tab, stmt,
			HISI_COMMON_FIELD_REGS_DUMP,
			(uint64_t)hevent.reg_msg, -1);
		db_eval_stmt(stmt, "hisi_common_section_tab");
	}

	return 0;
}

static struct ras_ns_ev_decoder hisi_section_ns_ev_decoder[] = {
	{
		.sec_type = "c8b328a8-9917-4af6-9a13-2e08ab2e7586",
		.decode = decode_hisi_common_section,
	},
};

static int hisi_ns_init(struct ras_module_ctx *ctx)
{
	unsigned int i;
	int rc;

	rc = ras_db_table_register(ctx, &hisi_common_section_db);
	if (rc)
		return rc;

	for (i = 0; i < ARRAY_SIZE(hisi_section_ns_ev_decoder); i++) {
		rc = register_ns_ev_decoder(&hisi_section_ns_ev_decoder[i]);
		if (rc) {
			ras_db_table_unregister(ctx);
			return rc;
		}
	}

	return 0;
}

static void hisi_ns_cleanup(struct ras_module_ctx *ctx)
{
	ras_db_table_unregister(ctx);
}

static const struct ras_module_entry hisi_ns_module = {
	.name = "non-standard-hisilicon",
	.level = SUB_EVENT_MODULE,
	.init = hisi_ns_init,
	.cleanup = hisi_ns_cleanup,
};

static void __attribute__((constructor)) hisi_ns_register(void)
{
	int rc = module_register(&hisi_ns_module);

	if (rc)
		log(TERM, LOG_ERR, "Failed to register HiSilicon module: %d\n", rc);
}

#ifdef HAVE_UNITTEST
struct db_table_descriptor_list hisilicon_table_descriptors(void)
{
	static const struct db_table_descriptor * const tables[] = {
		&hisi_common_section_tab,
	};

	return (struct db_table_descriptor_list) { tables, ARRAY_SIZE(tables) };
}
#endif
