/*
 * Copyright (c) 2019 Hisilicon Limited.
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
#include "ras-non-standard-handler.h"

/* HISI OEM error definitions */
/* HISI OEM format1 error definitions */
#define HISI_OEM_MODULE_ID_MN	0
#define HISI_OEM_MODULE_ID_PLL	1
#define HISI_OEM_MODULE_ID_SLLC	2
#define HISI_OEM_MODULE_ID_AA	3
#define HISI_OEM_MODULE_ID_SIOE	4
#define HISI_OEM_MODULE_ID_POE	5
#define HISI_OEM_MODULE_ID_DISP	8
#define HISI_OEM_MODULE_ID_LPC	9
#define HISI_OEM_MODULE_ID_GIC	13
#define HISI_OEM_MODULE_ID_RDE	14
#define HISI_OEM_MODULE_ID_SAS	15
#define HISI_OEM_MODULE_ID_SATA	16
#define HISI_OEM_MODULE_ID_USB	17

#define HISI_OEM_VALID_SOC_ID		BIT(0)
#define HISI_OEM_VALID_SOCKET_ID	BIT(1)
#define HISI_OEM_VALID_NIMBUS_ID	BIT(2)
#define HISI_OEM_VALID_MODULE_ID	BIT(3)
#define HISI_OEM_VALID_SUB_MODULE_ID	BIT(4)
#define HISI_OEM_VALID_ERR_SEVERITY	BIT(5)

#define HISI_OEM_TYPE1_VALID_ERR_MISC_0	BIT(6)
#define HISI_OEM_TYPE1_VALID_ERR_MISC_1	BIT(7)
#define HISI_OEM_TYPE1_VALID_ERR_MISC_2	BIT(8)
#define HISI_OEM_TYPE1_VALID_ERR_MISC_3	BIT(9)
#define HISI_OEM_TYPE1_VALID_ERR_MISC_4	BIT(10)
#define HISI_OEM_TYPE1_VALID_ERR_ADDR	BIT(11)

/* HISI OEM format2 error definitions */
#define HISI_OEM_MODULE_ID_SMMU	0
#define HISI_OEM_MODULE_ID_HHA	1
#define HISI_OEM_MODULE_ID_PA	2
#define HISI_OEM_MODULE_ID_HLLC	3
#define HISI_OEM_MODULE_ID_DDRC	4

#define HISI_OEM_TYPE2_VALID_ERR_FR	BIT(6)
#define HISI_OEM_TYPE2_VALID_ERR_CTRL	BIT(7)
#define HISI_OEM_TYPE2_VALID_ERR_STATUS	BIT(8)
#define HISI_OEM_TYPE2_VALID_ERR_ADDR	BIT(9)
#define HISI_OEM_TYPE2_VALID_ERR_MISC_0	BIT(10)
#define HISI_OEM_TYPE2_VALID_ERR_MISC_1	BIT(11)

/* HISI PCIe Local error definitions */
#define HISI_PCIE_SUB_MODULE_ID_AP	0
#define HISI_PCIE_SUB_MODULE_ID_TL	1
#define HISI_PCIE_SUB_MODULE_ID_MAC	2
#define HISI_PCIE_SUB_MODULE_ID_DL	3
#define HISI_PCIE_SUB_MODULE_ID_SDI	4

#define HISI_PCIE_LOCAL_VALID_VERSION		BIT(0)
#define HISI_PCIE_LOCAL_VALID_SOC_ID		BIT(1)
#define HISI_PCIE_LOCAL_VALID_SOCKET_ID		BIT(2)
#define HISI_PCIE_LOCAL_VALID_NIMBUS_ID		BIT(3)
#define HISI_PCIE_LOCAL_VALID_SUB_MODULE_ID	BIT(4)
#define HISI_PCIE_LOCAL_VALID_CORE_ID		BIT(5)
#define HISI_PCIE_LOCAL_VALID_PORT_ID		BIT(6)
#define HISI_PCIE_LOCAL_VALID_ERR_TYPE		BIT(7)
#define HISI_PCIE_LOCAL_VALID_ERR_SEVERITY	BIT(8)
#define HISI_PCIE_LOCAL_VALID_ERR_MISC		9

struct hisi_oem_type1_err_sec {
	uint32_t   val_bits;
	uint8_t    version;
	uint8_t    soc_id;
	uint8_t    socket_id;
	uint8_t    nimbus_id;
	uint8_t    module_id;
	uint8_t    sub_module_id;
	uint8_t    err_severity;
	uint8_t    reserv;
	uint32_t   err_misc_0;
	uint32_t   err_misc_1;
	uint32_t   err_misc_2;
	uint32_t   err_misc_3;
	uint32_t   err_misc_4;
	uint64_t   err_addr;
};

struct hisi_oem_type2_err_sec {
	uint32_t   val_bits;
	uint8_t    version;
	uint8_t    soc_id;
	uint8_t    socket_id;
	uint8_t    nimbus_id;
	uint8_t    module_id;
	uint8_t    sub_module_id;
	uint8_t    err_severity;
	uint8_t    reserv;
	uint32_t   err_fr_0;
	uint32_t   err_fr_1;
	uint32_t   err_ctrl_0;
	uint32_t   err_ctrl_1;
	uint32_t   err_status_0;
	uint32_t   err_status_1;
	uint32_t   err_addr_0;
	uint32_t   err_addr_1;
	uint32_t   err_misc0_0;
	uint32_t   err_misc0_1;
	uint32_t   err_misc1_0;
	uint32_t   err_misc1_1;
};

struct hisi_pcie_local_err_sec {
	uint64_t   val_bits;
	uint8_t    version;
	uint8_t    soc_id;
	uint8_t    socket_id;
	uint8_t    nimbus_id;
	uint8_t    sub_module_id;
	uint8_t    core_id;
	uint8_t    port_id;
	uint8_t    err_severity;
	uint16_t   err_type;
	uint8_t    reserv[2];
	uint32_t   err_misc[33];
};

enum hisi_oem_data_type {
	hisi_oem_data_type_int,
	hisi_oem_data_type_int64,
	hisi_oem_data_type_text,
};

enum {
	hip08_oem_type1_field_id,
	hip08_oem_type1_field_timestamp,
	hip08_oem_type1_field_version,
	hip08_oem_type1_field_soc_id,
	hip08_oem_type1_field_socket_id,
	hip08_oem_type1_field_nimbus_id,
	hip08_oem_type1_field_module_id,
	hip08_oem_type1_field_sub_module_id,
	hip08_oem_type1_field_err_sev,
	hip08_oem_type1_field_regs_dump,
};

enum {
	hip08_oem_type2_field_id,
	hip08_oem_type2_field_timestamp,
	hip08_oem_type2_field_version,
	hip08_oem_type2_field_soc_id,
	hip08_oem_type2_field_socket_id,
	hip08_oem_type2_field_nimbus_id,
	hip08_oem_type2_field_module_id,
	hip08_oem_type2_field_sub_module_id,
	hip08_oem_type2_field_err_sev,
	hip08_oem_type2_field_regs_dump,
};

enum {
	hip08_pcie_local_field_id,
	hip08_pcie_local_field_timestamp,
	hip08_pcie_local_field_version,
	hip08_pcie_local_field_soc_id,
	hip08_pcie_local_field_socket_id,
	hip08_pcie_local_field_nimbus_id,
	hip08_pcie_local_field_sub_module_id,
	hip08_pcie_local_field_core_id,
	hip08_pcie_local_field_port_id,
	hip08_pcie_local_field_err_sev,
	hip08_pcie_local_field_err_type,
	hip08_pcie_local_field_regs_dump,
};

/* helper functions */
static char *err_severity(uint8_t err_sev)
{
	switch (err_sev) {
	case 0: return "recoverable";
	case 1: return "fatal";
	case 2: return "corrected";
	case 3: return "none";
	}
	return "unknown";
}

static char *oem_type1_module_name(uint8_t module_id)
{
	switch (module_id) {
	case HISI_OEM_MODULE_ID_MN: return "MN";
	case HISI_OEM_MODULE_ID_PLL: return "PLL";
	case HISI_OEM_MODULE_ID_SLLC: return "SLLC";
	case HISI_OEM_MODULE_ID_AA: return "AA";
	case HISI_OEM_MODULE_ID_SIOE: return "SIOE";
	case HISI_OEM_MODULE_ID_POE: return "POE";
	case HISI_OEM_MODULE_ID_DISP: return "DISP";
	case HISI_OEM_MODULE_ID_LPC: return "LPC";
	case HISI_OEM_MODULE_ID_GIC: return "GIC";
	case HISI_OEM_MODULE_ID_RDE: return "RDE";
	case HISI_OEM_MODULE_ID_SAS: return "SAS";
	case HISI_OEM_MODULE_ID_SATA: return "SATA";
	case HISI_OEM_MODULE_ID_USB: return "USB";
	}
	return "unknown";
}

static char *oem_type2_module_name(uint8_t module_id)
{
	switch (module_id) {
	case HISI_OEM_MODULE_ID_SMMU: return "SMMU";
	case HISI_OEM_MODULE_ID_HHA: return "HHA";
	case HISI_OEM_MODULE_ID_HLLC: return "HLLC";
	case HISI_OEM_MODULE_ID_PA: return "PA";
	case HISI_OEM_MODULE_ID_DDRC: return "DDRC";
	}
	return "unknown module";
}

static const char *smmu_submodule_name[] = {
	"HAC_SMMU",
	"PCIE_SMMU",
	"MGMT_SMMU",
	"NIC_SMMU",
};

static const char *hllc_submodule_name[] = {
	"HLLC0",
	"HLLC1",
	"HLLC2",
};

static const char *hha_submodule_name[] = {
	"TA_HHA0",
	"TA_HHA1",
	"TB_HHA0",
	"TB_HHA1"
};

static const char *ddrc_submodule_name[] = {
	"TA_DDRC0",
	"TA_DDRC1",
	"TA_DDRC2",
	"TA_DDRC3",
	"TB_DDRC0",
	"TB_DDRC1",
	"TB_DDRC2",
	"TB_DDRC3",
};

static const char *oem_type2_sub_module_name(uint8_t module_id, uint8_t sub_module_id)
{
	switch (module_id) {
	case HISI_OEM_MODULE_ID_SMMU:
		if (sub_module_id < sizeof(smmu_submodule_name)/sizeof(char *))
			return smmu_submodule_name[sub_module_id];
		break;
	case HISI_OEM_MODULE_ID_HLLC:
		if (sub_module_id < sizeof(hllc_submodule_name)/sizeof(char *))
			return hllc_submodule_name[sub_module_id];
		break;
	case HISI_OEM_MODULE_ID_PA:
		return "PA";
	case HISI_OEM_MODULE_ID_HHA:
		if (sub_module_id < sizeof(hha_submodule_name)/sizeof(char *))
			return hha_submodule_name[sub_module_id];
		break;
	case HISI_OEM_MODULE_ID_DDRC:
		if (sub_module_id < sizeof(ddrc_submodule_name)/sizeof(char *))
			return ddrc_submodule_name[sub_module_id];
		break;
	}

	return "unknown";
}

static char *pcie_local_sub_module_name(uint8_t id)
{
	switch (id) {
	case HISI_PCIE_SUB_MODULE_ID_AP: return "AP_Layer";
	case HISI_PCIE_SUB_MODULE_ID_TL: return "TL_Layer";
	case HISI_PCIE_SUB_MODULE_ID_MAC: return "MAC_Layer";
	case HISI_PCIE_SUB_MODULE_ID_DL: return "DL_Layer";
	case HISI_PCIE_SUB_MODULE_ID_SDI: return "SDI_Layer";
	}
	return "unknown";
}

#ifdef HAVE_SQLITE3
static const struct db_fields hip08_oem_type1_event_fields[] = {
	{ .name = "id",			.type = "INTEGER PRIMARY KEY" },
	{ .name = "timestamp",          .type = "TEXT" },
	{ .name = "version",		.type = "INTEGER" },
	{ .name = "soc_id",		.type = "INTEGER" },
	{ .name = "socket_id",		.type = "INTEGER" },
	{ .name = "nimbus_id",		.type = "INTEGER" },
	{ .name = "module_id",		.type = "TEXT" },
	{ .name = "sub_module_id",	.type = "TEXT" },
	{ .name = "err_severity",	.type = "TEXT" },
	{ .name = "regs_dump",		.type = "TEXT" },
};

static const struct db_table_descriptor hip08_oem_type1_event_tab = {
	.name = "hip08_oem_type1_event_v2",
	.fields = hip08_oem_type1_event_fields,
	.num_fields = ARRAY_SIZE(hip08_oem_type1_event_fields),
};

static const struct db_fields hip08_oem_type2_event_fields[] = {
	{ .name = "id",                 .type = "INTEGER PRIMARY KEY" },
	{ .name = "timestamp",          .type = "TEXT" },
	{ .name = "version",            .type = "INTEGER" },
	{ .name = "soc_id",             .type = "INTEGER" },
	{ .name = "socket_id",          .type = "INTEGER" },
	{ .name = "nimbus_id",          .type = "INTEGER" },
	{ .name = "module_id",          .type = "TEXT" },
	{ .name = "sub_module_id",      .type = "TEXT" },
	{ .name = "err_severity",       .type = "TEXT" },
	{ .name = "regs_dump",		.type = "TEXT" },
};

static const struct db_table_descriptor hip08_oem_type2_event_tab = {
	.name = "hip08_oem_type2_event_v2",
	.fields = hip08_oem_type2_event_fields,
	.num_fields = ARRAY_SIZE(hip08_oem_type2_event_fields),
};

static const struct db_fields hip08_pcie_local_event_fields[] = {
	{ .name = "id",                 .type = "INTEGER PRIMARY KEY" },
	{ .name = "timestamp",          .type = "TEXT" },
	{ .name = "version",            .type = "INTEGER" },
	{ .name = "soc_id",             .type = "INTEGER" },
	{ .name = "socket_id",          .type = "INTEGER" },
	{ .name = "nimbus_id",          .type = "INTEGER" },
	{ .name = "sub_module_id",      .type = "TEXT" },
	{ .name = "core_id",		.type = "INTEGER" },
	{ .name = "port_id",		.type = "INTEGER" },
	{ .name = "err_severity",       .type = "TEXT" },
	{ .name = "err_type",		.type = "INTEGER" },
	{ .name = "regs_dump",		.type = "TEXT" },
};

static const struct db_table_descriptor hip08_pcie_local_event_tab = {
	.name = "hip08_pcie_local_event_v2",
	.fields = hip08_pcie_local_event_fields,
	.num_fields = ARRAY_SIZE(hip08_pcie_local_event_fields),
};

static void record_vendor_data(struct ras_ns_dec_tab *dec_tab,
			       enum hisi_oem_data_type data_type,
			       int id, int64_t data, const char *text)
{
	switch (data_type) {
	case hisi_oem_data_type_int:
		sqlite3_bind_int(dec_tab->stmt_dec_record, id, data);
		break;
	case hisi_oem_data_type_int64:
		sqlite3_bind_int64(dec_tab->stmt_dec_record, id, data);
		break;
	case hisi_oem_data_type_text:
		sqlite3_bind_text(dec_tab->stmt_dec_record, id, text, -1, NULL);
		break;
	default:
		break;
	}
}

static int step_vendor_data_tab(struct ras_ns_dec_tab *dec_tab, char *name)
{
	int rc;

	rc = sqlite3_step(dec_tab->stmt_dec_record);
	if (rc != SQLITE_OK && rc != SQLITE_DONE)
		log(TERM, LOG_ERR,
		    "Failed to do %s step on sqlite: error = %d\n", name, rc);

	rc = sqlite3_reset(dec_tab->stmt_dec_record);
	if (rc != SQLITE_OK && rc != SQLITE_DONE)
		log(TERM, LOG_ERR,
		    "Failed to reset %s on sqlite: error = %d\n", name, rc);

	rc = sqlite3_clear_bindings(dec_tab->stmt_dec_record);
	if (rc != SQLITE_OK && rc != SQLITE_DONE)
		log(TERM, LOG_ERR,
		    "Failed to clear bindings %s on sqlite: error = %d\n",
		    name, rc);

	return rc;
}
#else
static void record_vendor_data(struct ras_ns_dec_tab *dec_tab,
			       enum hisi_oem_data_type data_type,
			       int id, int64_t data, const char *text)
{ }

static int step_vendor_data_tab(struct ras_ns_dec_tab *dec_tab, char *name)
{
	return 0;
}
#endif

/* error data decoding functions */
static int decode_hip08_oem_type1_error(struct ras_events *ras,
					struct ras_ns_dec_tab *dec_tab,
					struct trace_seq *s,
					struct ras_non_standard_event *event)
{
	const struct hisi_oem_type1_err_sec *err =
			(struct hisi_oem_type1_err_sec*)event->error;
	char buf[1024];
	char *p = buf;

	if (err->val_bits == 0) {
		trace_seq_printf(s, "%s: no valid error information\n",
				 __func__);
		return -1;
	}

#ifdef HAVE_SQLITE3
	if (!dec_tab->stmt_dec_record) {
		if (ras_mc_add_vendor_table(ras, &dec_tab->stmt_dec_record,
					    &hip08_oem_type1_event_tab)
			!= SQLITE_OK) {
			trace_seq_printf(s,
					"create sql hip08_oem_type1_event_tab fail\n");
			return -1;
		}
	}
#endif
	record_vendor_data(dec_tab, hisi_oem_data_type_text,
			   hip08_oem_type1_field_timestamp,
			   0, event->timestamp);

	p += sprintf(p, "[ ");
	p += sprintf(p, "table_version=%d ", err->version);
	record_vendor_data(dec_tab, hisi_oem_data_type_int,
			   hip08_oem_type1_field_version, err->version, NULL);

	if (err->val_bits & HISI_OEM_VALID_SOC_ID) {
		p += sprintf(p, "SOC_ID=%d ", err->soc_id);
		record_vendor_data(dec_tab, hisi_oem_data_type_int,
				   hip08_oem_type1_field_soc_id,
				   err->soc_id, NULL);
	}

	if (err->val_bits & HISI_OEM_VALID_SOCKET_ID) {
		p += sprintf(p, "socket_ID=%d ", err->socket_id);
		record_vendor_data(dec_tab, hisi_oem_data_type_int,
				   hip08_oem_type1_field_socket_id,
				   err->socket_id, NULL);
	}

	if (err->val_bits & HISI_OEM_VALID_NIMBUS_ID) {
		p += sprintf(p, "nimbus_ID=%d ", err->nimbus_id);
		record_vendor_data(dec_tab, hisi_oem_data_type_int,
				   hip08_oem_type1_field_nimbus_id,
				   err->nimbus_id, NULL);
	}

	if (err->val_bits & HISI_OEM_VALID_MODULE_ID) {
		p += sprintf(p, "module=%s ",
			     oem_type1_module_name(err->module_id));
		record_vendor_data(dec_tab, hisi_oem_data_type_text,
				   hip08_oem_type1_field_module_id,
				   0, oem_type1_module_name(err->module_id));
	}

	if (err->val_bits & HISI_OEM_VALID_SUB_MODULE_ID) {
		char submodule_name[64];

		sprintf(submodule_name, "%s%d",
			oem_type1_module_name(err->module_id),
			err->sub_module_id);
		p += sprintf(p, "submodule=%s ", submodule_name);
		record_vendor_data(dec_tab, hisi_oem_data_type_text,
				   hip08_oem_type1_field_sub_module_id,
				   0, submodule_name);
	}

	if (err->val_bits & HISI_OEM_VALID_ERR_SEVERITY) {
		p += sprintf(p, "error_severity=%s ",
			     err_severity(err->err_severity));
		record_vendor_data(dec_tab, hisi_oem_data_type_text,
				   hip08_oem_type1_field_err_sev,
				   0, err_severity(err->err_severity));
	}

	p += sprintf(p, "]");
	trace_seq_printf(s, "\nHISI HIP08: OEM Type-1 Error\n");
	trace_seq_printf(s, "%s\n", buf);

	p = buf;
	trace_seq_printf(s, "Reg Dump:\n");
	if (err->val_bits & HISI_OEM_TYPE1_VALID_ERR_MISC_0) {
		trace_seq_printf(s, "ERR_MISC0=0x%x\n", err->err_misc_0);
		p += sprintf(p, "ERR_MISC0=0x%x ", err->err_misc_0);
	}

	if (err->val_bits & HISI_OEM_TYPE1_VALID_ERR_MISC_1) {
		trace_seq_printf(s, "ERR_MISC1=0x%x\n", err->err_misc_1);
		p += sprintf(p, "ERR_MISC1=0x%x ", err->err_misc_1);
	}

	if (err->val_bits & HISI_OEM_TYPE1_VALID_ERR_MISC_2) {
		trace_seq_printf(s, "ERR_MISC2=0x%x\n", err->err_misc_2);
		p += sprintf(p, "ERR_MISC2=0x%x ", err->err_misc_2);
	}

	if (err->val_bits & HISI_OEM_TYPE1_VALID_ERR_MISC_3) {
		trace_seq_printf(s, "ERR_MISC3=0x%x\n", err->err_misc_3);
		p += sprintf(p, "ERR_MISC3=0x%x ", err->err_misc_3);
	}

	if (err->val_bits & HISI_OEM_TYPE1_VALID_ERR_MISC_4) {
		trace_seq_printf(s, "ERR_MISC4=0x%x\n", err->err_misc_4);
		p += sprintf(p, "ERR_MISC4=0x%x ", err->err_misc_4);
	}

	if (err->val_bits & HISI_OEM_TYPE1_VALID_ERR_ADDR) {
		trace_seq_printf(s, "ERR_ADDR=0x%p\n", (void *)err->err_addr);
		p += sprintf(p, "ERR_ADDR=0x%p ", (void *)err->err_addr);
	}

	*(--p) = '\0';
	record_vendor_data(dec_tab, hisi_oem_data_type_text,
			   hip08_oem_type1_field_regs_dump, 0, buf);

	step_vendor_data_tab(dec_tab, "hip08_oem_type1_event_tab");

	return 0;
}

static int decode_hip08_oem_type2_error(struct ras_events *ras,
					struct ras_ns_dec_tab *dec_tab,
					struct trace_seq *s,
					struct ras_non_standard_event *event)
{
	const struct hisi_oem_type2_err_sec *err =
			(struct hisi_oem_type2_err_sec *)event->error;
	char buf[1024];
	char *p = buf;

	if (err->val_bits == 0) {
		trace_seq_printf(s, "%s: no valid error information\n",
				 __func__);
		return -1;
	}

#ifdef HAVE_SQLITE3
	if (!dec_tab->stmt_dec_record) {
		if (ras_mc_add_vendor_table(ras, &dec_tab->stmt_dec_record,
			&hip08_oem_type2_event_tab) != SQLITE_OK) {
			trace_seq_printf(s,
				"create sql hip08_oem_type2_event_tab fail\n");
			return -1;
		}
	}
#endif
	record_vendor_data(dec_tab, hisi_oem_data_type_text,
			   hip08_oem_type2_field_timestamp,
			   0, event->timestamp);

	p += sprintf(p, "[ ");
	p += sprintf(p, "table_version=%d ", err->version);
	record_vendor_data(dec_tab, hisi_oem_data_type_int,
			   hip08_oem_type2_field_version,
			   err->version, NULL);
	if (err->val_bits & HISI_OEM_VALID_SOC_ID) {
		p += sprintf(p, "SOC_ID=%d ", err->soc_id);
		record_vendor_data(dec_tab, hisi_oem_data_type_int,
				   hip08_oem_type2_field_soc_id,
				   err->soc_id, NULL);
	}

	if (err->val_bits & HISI_OEM_VALID_SOCKET_ID) {
		p += sprintf(p, "socket_ID=%d ", err->socket_id);
		record_vendor_data(dec_tab, hisi_oem_data_type_int,
				   hip08_oem_type2_field_socket_id,
				   err->socket_id, NULL);
	}

	if (err->val_bits & HISI_OEM_VALID_NIMBUS_ID) {
		p += sprintf(p, "nimbus_ID=%d ", err->nimbus_id);
		record_vendor_data(dec_tab, hisi_oem_data_type_int,
				   hip08_oem_type2_field_nimbus_id,
				   err->nimbus_id, NULL);
	}

	if (err->val_bits & HISI_OEM_VALID_MODULE_ID) {
		p += sprintf(p, "module=%s ",
			     oem_type2_module_name(err->module_id));
		record_vendor_data(dec_tab, hisi_oem_data_type_text,
				   hip08_oem_type2_field_module_id,
				   0, oem_type2_module_name(err->module_id));
	}

	if (err->val_bits & HISI_OEM_VALID_SUB_MODULE_ID) {
		const char *str = oem_type2_sub_module_name(err->module_id,
							    err->sub_module_id);

		p += sprintf(p, "submodule=%s ", str);
		record_vendor_data(dec_tab, hisi_oem_data_type_text,
				   hip08_oem_type2_field_sub_module_id,
				   0, str);
	}

	if (err->val_bits & HISI_OEM_VALID_ERR_SEVERITY) {
		p += sprintf(p, "error_severity=%s ",
			     err_severity(err->err_severity));
		record_vendor_data(dec_tab, hisi_oem_data_type_text,
				   hip08_oem_type2_field_err_sev,
				   0, err_severity(err->err_severity));
	}

	p += sprintf(p, "]");
	trace_seq_printf(s, "\nHISI HIP08: OEM Type-2 Error\n");
	trace_seq_printf(s, "%s\n", buf);

	p = buf;
	trace_seq_printf(s, "Reg Dump:\n");
	if (err->val_bits & HISI_OEM_TYPE2_VALID_ERR_FR) {
		trace_seq_printf(s, "ERR_FR_0=0x%x\n", err->err_fr_0);
		trace_seq_printf(s, "ERR_FR_1=0x%x\n", err->err_fr_1);
		p += sprintf(p, "ERR_FR_0=0x%x ERR_FR_1=0x%x ",
			     err->err_fr_0, err->err_fr_1);
	}

	if (err->val_bits & HISI_OEM_TYPE2_VALID_ERR_CTRL) {
		trace_seq_printf(s, "ERR_CTRL_0=0x%x\n", err->err_ctrl_0);
		trace_seq_printf(s, "ERR_CTRL_1=0x%x\n", err->err_ctrl_1);
		p += sprintf(p, "ERR_CTRL_0=0x%x ERR_CTRL_1=0x%x ",
				err->err_ctrl_0, err->err_ctrl_1);
	}

	if (err->val_bits & HISI_OEM_TYPE2_VALID_ERR_STATUS) {
		trace_seq_printf(s, "ERR_STATUS_0=0x%x\n", err->err_status_0);
		trace_seq_printf(s, "ERR_STATUS_1=0x%x\n", err->err_status_1);
		p += sprintf(p, "ERR_STATUS_0=0x%x ERR_STATUS_1=0x%x ",
			     err->err_status_0, err->err_status_1);
	}

	if (err->val_bits & HISI_OEM_TYPE2_VALID_ERR_ADDR) {
		trace_seq_printf(s, "ERR_ADDR_0=0x%x\n", err->err_addr_0);
		trace_seq_printf(s, "ERR_ADDR_1=0x%x\n", err->err_addr_1);
		p += sprintf(p, "ERR_ADDR_0=0x%x ERR_ADDR_1=0x%x ",
			     err->err_addr_0, err->err_addr_1);
	}

	if (err->val_bits & HISI_OEM_TYPE2_VALID_ERR_MISC_0) {
		trace_seq_printf(s, "ERR_MISC0_0=0x%x\n", err->err_misc0_0);
		trace_seq_printf(s, "ERR_MISC0_1=0x%x\n", err->err_misc0_1);
		p += sprintf(p, "ERR_MISC0_0=0x%x ERR_MISC0_1=0x%x ",
			     err->err_misc0_0, err->err_misc0_1);
	}

	if (err->val_bits & HISI_OEM_TYPE2_VALID_ERR_MISC_1) {
		trace_seq_printf(s, "ERR_MISC1_0=0x%x\n", err->err_misc1_0);
		trace_seq_printf(s, "ERR_MISC1_1=0x%x\n", err->err_misc1_1);
		p += sprintf(p, "ERR_MISC1_0=0x%x ERR_MISC1_1=0x%x ",
			     err->err_misc1_0, err->err_misc1_1);
	}

	*(--p) = '\0';
	record_vendor_data(dec_tab, hisi_oem_data_type_text,
			   hip08_oem_type2_field_regs_dump, 0, buf);

	step_vendor_data_tab(dec_tab, "hip08_oem_type2_event_tab");

	return 0;
}

static int decode_hip08_pcie_local_error(struct ras_events *ras,
					 struct ras_ns_dec_tab *dec_tab,
					 struct trace_seq *s,
					 struct ras_non_standard_event *event)
{
	const struct hisi_pcie_local_err_sec *err =
			(struct hisi_pcie_local_err_sec *)event->error;
	char buf[1024];
	char *p = buf;
	uint32_t i;

	if (err->val_bits == 0) {
		trace_seq_printf(s, "%s: no valid error information\n",
				 __func__);
		return -1;
	}

#ifdef HAVE_SQLITE3
	if (!dec_tab->stmt_dec_record) {
		if (ras_mc_add_vendor_table(ras, &dec_tab->stmt_dec_record,
				&hip08_pcie_local_event_tab) != SQLITE_OK) {
			trace_seq_printf(s,
				"create sql hip08_pcie_local_event_tab fail\n");
			return -1;
		}
	}
#endif
	record_vendor_data(dec_tab, hisi_oem_data_type_text,
			   hip08_pcie_local_field_timestamp,
			   0, event->timestamp);

	p += sprintf(p, "[ ");
	p += sprintf(p, "table_version=%d ", err->version);
	record_vendor_data(dec_tab, hisi_oem_data_type_int,
			   hip08_pcie_local_field_version,
			   err->version, NULL);
	if (err->val_bits & HISI_PCIE_LOCAL_VALID_SOC_ID) {
		p += sprintf(p, "SOC_ID=%d ", err->soc_id);
		record_vendor_data(dec_tab, hisi_oem_data_type_int,
				   hip08_pcie_local_field_soc_id,
				   err->soc_id, NULL);
	}

	if (err->val_bits & HISI_PCIE_LOCAL_VALID_SOCKET_ID) {
		p += sprintf(p, "socket_ID=%d ", err->socket_id);
		record_vendor_data(dec_tab, hisi_oem_data_type_int,
				   hip08_pcie_local_field_socket_id,
				   err->socket_id, NULL);
	}

	if (err->val_bits & HISI_PCIE_LOCAL_VALID_NIMBUS_ID) {
		p += sprintf(p, "nimbus_ID=%d ", err->nimbus_id);
		record_vendor_data(dec_tab, hisi_oem_data_type_int,
				   hip08_pcie_local_field_nimbus_id,
				   err->nimbus_id, NULL);
	}

	if (err->val_bits & HISI_PCIE_LOCAL_VALID_SUB_MODULE_ID) {
		p += sprintf(p, "submodule=%s ",
			     pcie_local_sub_module_name(err->sub_module_id));
		record_vendor_data(dec_tab, hisi_oem_data_type_text,
				   hip08_pcie_local_field_sub_module_id,
				   0, pcie_local_sub_module_name(err->sub_module_id));
	}

	if (err->val_bits & HISI_PCIE_LOCAL_VALID_CORE_ID) {
		p += sprintf(p, "core_ID=core%d ", err->core_id);
		record_vendor_data(dec_tab, hisi_oem_data_type_int,
				   hip08_pcie_local_field_core_id,
				   err->core_id, NULL);
	}

	if (err->val_bits & HISI_PCIE_LOCAL_VALID_PORT_ID) {
		p += sprintf(p, "port_ID=port%d ", err->port_id);
		record_vendor_data(dec_tab, hisi_oem_data_type_int,
				   hip08_pcie_local_field_port_id,
				   err->port_id, NULL);
	}

	if (err->val_bits & HISI_PCIE_LOCAL_VALID_ERR_SEVERITY) {
		p += sprintf(p, "error_severity=%s ",
			     err_severity(err->err_severity));
		record_vendor_data(dec_tab, hisi_oem_data_type_text,
				   hip08_pcie_local_field_err_sev,
				   0, err_severity(err->err_severity));
	}

	if (err->val_bits & HISI_PCIE_LOCAL_VALID_ERR_TYPE) {
		p += sprintf(p, "error_type=0x%x ", err->err_type);
		record_vendor_data(dec_tab, hisi_oem_data_type_int,
				   hip08_pcie_local_field_err_type,
				   err->err_type, NULL);
	}
	p += sprintf(p, "]");

	trace_seq_printf(s, "\nHISI HIP08: PCIe local error\n");
	trace_seq_printf(s, "%s\n", buf);

	p = buf;
	trace_seq_printf(s, "Reg Dump:\n");
	for (i = 0; i < 33; i++) {
		if (err->val_bits & BIT(HISI_PCIE_LOCAL_VALID_ERR_MISC + i)) {
			trace_seq_printf(s, "ERR_MISC_%d=0x%x\n", i,
					 err->err_misc[i]);
			p += sprintf(p, "ERR_MISC_%d=0x%x ", i, err->err_misc[i]);
		}
	}

	*(--p) = '\0';
	record_vendor_data(dec_tab, hisi_oem_data_type_text,
			   hip08_pcie_local_field_regs_dump, 0, buf);

	step_vendor_data_tab(dec_tab, "hip08_pcie_local_event_tab");

	return 0;
}

struct ras_ns_dec_tab hip08_ns_oem_tab[] = {
	{
		.sec_type = "1f8161e155d641e6bd107afd1dc5f7c5",
		.decode = decode_hip08_oem_type1_error,
	},
	{
		.sec_type = "45534ea6ce2341158535e07ab3aef91d",
		.decode = decode_hip08_oem_type2_error,
	},
	{
		.sec_type = "b2889fc9e7d74f9da867af42e98be772",
		.decode = decode_hip08_pcie_local_error,
	},
	{ /* sentinel */ }
};

__attribute__((constructor))
static void hip08_init(void)
{
	register_ns_dec_tab(hip08_ns_oem_tab);
}
