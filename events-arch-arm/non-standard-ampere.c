// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * Copyright (c) 2020, Ampere Computing LLC.
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/ras-logger.h"
#include "core/modules.h"
#include "core/types.h"
#include "events-arch-arm/non-standard-ampere.h"
#include "events-arch-arm/ras-non-standard-handler.h"
#include "modules/ras-report.h"

/*Armv8 RAS compicant Error Record(APEI and BMC Reporting) Payload Type 0*/
static const char * const disp_payload0_err_reg_name[] = {
	"Error Type:",
	"Error SubType:",
	"Error Instance:",
	"Processor Socket:",
	"Status:",
	"Address:",
	"MISC0:",
	"MISC1:",
	"MISC2:",
	"MISC3:",
};

/*PCIe AER Error Payload Type 1*/
static const char * const disp_payload1_err_reg_name[] = {
	"Error Type:",
	"Error Subtype:",
	"Error Instance:",
	"Processor Socket:",
	"AER_UNCORR_ERR_STATUS:",
	"AER_UNCORR_ERR_MASK:",
	"AER_UNCORR_ERR_SEV:",
	"AER_CORR_ERR_STATUS:",
	"AER_CORR_ERR_MASK:",
	"AER_ROOT_ERR_CMD:",
	"AER_ROOT_ERR_STATUS:",
	"AER_ERR_SRC_ID:",
	"Reserved:",
	"Reserved:",
};

/*PCIe RAS Dat Path(RASDP), Payload Type 2 */
static const char * const disp_payload2_err_reg_name[] = {
	"Error Type:",
	"Error Subtype:",
	"Error Instance:",
	"Processor Socket:",
	"CE Report Register:",
	"CE Location Register:",
	"CE Address:",
	"UE Reprot Register:",
	"UE Location Register:",
	"UE Address:",
	"Reserved:",
	"Reserved:",
	"Reserved:",
};

/*Firmware-Specific Data(ATF, SMPro, PMpro, and BERT), Payload Type 3 */
static const char * const disp_payload3_err_reg_name[] = {
	"Error Type:",
	"Error Subtype:",
	"Error Instance:",
	"Processor Socket:",
	"Firmware-Specific Data 0:",
	"Firmware-Specific Data 1:",
	"Firmware-Specific Data 2:",
	"Firmware-Specific Data 3:",
	"Firmware-Specific Data 4:",
	"Firmware-Specific Data 5:",
};

static const char * const err_cpm_sub_type[] = {
	"Snoop-Logic",
	"ARMv8 Core 0",
	"ARMv8 Core 1",
};

static const char * const err_mcu_sub_type[] = {
	"ERR0",
	"ERR1",
	"ERR2",
	"ERR3",
	"ERR4",
	"ERR5",
	"ERR6",
	"Link Error",
};

static const char * const err_mesh_sub_type[] = {
	"Cross Point",
	"Home Node(IO)",
	"Home Node(Memory)",
	"CCIX Node",
};

static const char * const err_2p_link_ms_sub_type[] = {
	"ERR0",
	"ERR1",
	"ERR2",
	"ERR3",
};

static const char * const err_gic_sub_type[] = {
	"ERR0",
	"ERR1",
	"ERR2",
	"ERR3",
	"ERR4",
	"ERR5",
	"ERR6",
	"ERR7",
	"ERR8",
	"ERR9",
	"ERR10",
	"ERR11",
	"ERR12",
	"ERR13(GIC ITS 0)",
	"ERR14(GIC ITS 1)",
	"ERR15(GIC ITS 2)",
	"ERR16(GIC ITS 3)",
	"ERR17(GIC ITS 4)",
	"ERR18(GIC ITS 5)",
	"ERR19(GIC ITS 6)",
	"ERR20(GIC ITS 7)",
};

/*as the SMMU's subtype value is consistent, using switch for type0*/
static char *err_smmu_sub_type(int etype)
{
	switch (etype) {
	case 0x00: return "TBU0";
	case 0x01: return "TBU1";
	case 0x02: return "TBU2";
	case 0x03: return "TBU3";
	case 0x04: return "TBU4";
	case 0x05: return "TBU5";
	case 0x06: return "TBU6";
	case 0x07: return "TBU7";
	case 0x08: return "TBU8";
	case 0x09: return "TBU9";
	case 0x64: return "TCU";
	}
	return "unknown error";
}

static const char * const err_pcie_aer_sub_type[] = {
	"Root Port",
	"Device",
};

/*as the PCIe RASDP's subtype value is consistent, using switch for type0/2*/
static char *err_peci_rasdp_sub_type(int etype)
{
	switch (etype) {
	case 0x00: return "RCA HB Error";
	case 0x01: return "RCB HB Error";
	case 0x08: return "RASDP Error";
	}
	return "unknown error";
}

static const char * const err_ocm_sub_type[] = {
	"ERR0",
	"ERR1",
	"ERR2",
};

static const char * const err_smpro_sub_type[] = {
	"ERR0",
	"ERR1",
	"MPA_ERR",
};

static const char * const err_pmpro_sub_type[] = {
	"ERR0",
	"ERR1",
	"MPA_ERR",
};

static const char * const err_atf_fw_sub_type[] = {
	"EL3",
	"SPM",
	"Secure Partition(SEL0/SEL1)",
};

static const char * const err_smpro_fw_sub_type[] = {
	"RAS_MSG_ERR",
	"",
};

static const char * const err_pmpro_fw_sub_type[] = {
	"RAS_MSG_ERR",
	"",
};

static const char * const err_bert_sub_type[] = {
	"Default",
	"Watchdog",
	"ATF Fatal",
	"SMPRO Fatal",
	"PMPRO Fatal",
};

struct amp_ras_type_info {
	int id;
	const char *name;
	const char * const *sub;
	int sub_num;
};

static const struct amp_ras_type_info amp_payload_error_type[] = {
	{
		.id = AMP_RAS_TYPE_CPU,
		.name = "CPM",
		.sub = err_cpm_sub_type,
		.sub_num = ARRAY_SIZE(err_cpm_sub_type),
	},
	{
		.id = AMP_RAS_TYPE_MCU,
		.name = "MCU",
		.sub = err_mcu_sub_type,
		.sub_num = ARRAY_SIZE(err_mcu_sub_type),
	},
	{
		.id = AMP_RAS_TYPE_MESH,
		.name = "MESH",
		.sub = err_mesh_sub_type,
		.sub_num = ARRAY_SIZE(err_mesh_sub_type),
	},
	{
		.id = AMP_RAS_TYPE_2P_LINK_QS,
		.name = "2P Link(Altra)",
	},
	{
		.id = AMP_RAS_TYPE_2P_LINK_MQ,
		.name = "2P Link(Altra Max)",
		.sub = err_2p_link_ms_sub_type,
		.sub_num = ARRAY_SIZE(err_2p_link_ms_sub_type),
	},
	{
		.id = AMP_RAS_TYPE_GIC,
		.name = "GIC",
		.sub = err_gic_sub_type,
		.sub_num = ARRAY_SIZE(err_gic_sub_type),
	},
	{
		.id = AMP_RAS_TYPE_SMMU,
		.name = "SMMU",
	},
	{
		.id = AMP_RAS_TYPE_PCIE_AER,
		.name = "PCIe AER",
		.sub = err_pcie_aer_sub_type,
		.sub_num = ARRAY_SIZE(err_pcie_aer_sub_type),
	},
	{
		.id = AMP_RAS_TYPE_PCIE_RASDP,
		.name = "PCIe RASDP",
	},
	{
		.id = AMP_RAS_TYPE_OCM,
		.name = "OCM",
		.sub = err_ocm_sub_type,
		.sub_num = ARRAY_SIZE(err_ocm_sub_type),
	},
	{
		.id = AMP_RAS_TYPE_SMPRO,
		.name = "SMPRO",
		.sub = err_smpro_sub_type,
		.sub_num = ARRAY_SIZE(err_smpro_sub_type),
	},
	{
		.id = AMP_RAS_TYPE_PMPRO,
		.name = "PMPRO",
		.sub = err_pmpro_sub_type,
		.sub_num = ARRAY_SIZE(err_pmpro_sub_type),
	},
	{
		.id = AMP_RAS_TYPE_ATF_FW,
		.name = "ATF FW",
		.sub = err_atf_fw_sub_type,
		.sub_num = ARRAY_SIZE(err_atf_fw_sub_type),
	},
	{
		.id = AMP_RAS_TYPE_SMPRO_FW,
		.name = "SMPRO FW",
		.sub = err_smpro_fw_sub_type,
		.sub_num = ARRAY_SIZE(err_smpro_fw_sub_type),
	},
	{
		.id = AMP_RAS_TYPE_PMPRO_FW,
		.name = "PMPRO FW",
		.sub = err_pmpro_fw_sub_type,
		.sub_num = ARRAY_SIZE(err_pmpro_fw_sub_type),
	},
	{
		.id = AMP_RAS_TYPE_BERT,
		.name = "BERT",
		.sub = err_bert_sub_type,
		.sub_num = ARRAY_SIZE(err_bert_sub_type),
	},
	{
	}
};

/*get the error type name*/
static const char *oem_type_name(const struct amp_ras_type_info *info,
				 uint8_t type_id)
{
	const struct amp_ras_type_info *type = &info[0];

	for (; type->name; type++) {
		if (type->id != type_id)
			continue;
		return type->name;
	}
	return "unknown";
}

/*get the error subtype*/
static const char *oem_subtype_name(const struct amp_ras_type_info *info,
				    uint8_t type_id, uint8_t sub_type_id)
{
	const struct amp_ras_type_info *type = &info[0];

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

/*key pair definition for ampere specific error payload type 0*/
static const struct db_fields amp_payload0_event_fields[] = {
	{ .name = "id",			.type = DB_TYPE_SERIAL, .is_pk = true },
	{ .name = "timestamp",          .type = DB_TYPE_TIMESTAMP },
	{ .name = "type",		.type = DB_TYPE_TEXT },
	{ .name = "subtype",		.type = DB_TYPE_TEXT },
	{ .name = "instance",		.type = DB_TYPE_INT32 },
	{ .name = "socket_num",		.type = DB_TYPE_INT32 },
	{ .name = "status_reg",		.type = DB_TYPE_INT64 },
	{ .name = "addr_reg",		.type = DB_TYPE_INT64 },
	{ .name = "misc0",		.type = DB_TYPE_INT64 },
	{ .name = "misc1",		.type = DB_TYPE_INT64 },
	{ .name = "misc2",		.type = DB_TYPE_INT64 },
	{ .name = "misc3",		.type = DB_TYPE_INT64 },
};

static const struct db_table_descriptor amp_payload0_event_tab = {
	.name = "amp_payload0_event",
	.fields = amp_payload0_event_fields,
	.num_fields = ARRAY_SIZE(amp_payload0_event_fields),
};

/*key pair definition for ampere specific error payload type 1*/
static const struct db_fields amp_payload1_event_fields[] = {
	{ .name = "id",			.type = DB_TYPE_SERIAL, .is_pk = true },
	{ .name = "timestamp",          .type = DB_TYPE_TIMESTAMP },
	{ .name = "type",		.type = DB_TYPE_TEXT },
	{ .name = "subtype",		.type = DB_TYPE_TEXT },
	{ .name = "instance",		.type = DB_TYPE_INT32 },
	{ .name = "socket_num",		.type = DB_TYPE_INT32 },
	{ .name = "uncore_err_status",	.type = DB_TYPE_INT32 },
	{ .name = "uncore_err_mask",	.type = DB_TYPE_INT32 },
	{ .name = "uncore_err_sev",	.type = DB_TYPE_INT32 },
	{ .name = "core_err_status",	.type = DB_TYPE_INT32 },
	{ .name = "core_err_mask",	.type = DB_TYPE_INT32 },
	{ .name = "root_err_cmd",	.type = DB_TYPE_INT32 },
	{ .name = "root_err_status",	.type = DB_TYPE_INT32 },
	{ .name = "src_id",		.type = DB_TYPE_INT32 },
	{ .name = "reserved1",		.type = DB_TYPE_INT32 },
	{ .name = "reserverd2",		.type = DB_TYPE_INT64 },
};

static const struct db_table_descriptor amp_payload1_event_tab = {
	.name = "amp_payload1_event",
	.fields = amp_payload1_event_fields,
	.num_fields = ARRAY_SIZE(amp_payload1_event_fields),
};

/*key pair definition for ampere specific error payload type 2*/
static const struct db_fields amp_payload2_event_fields[] = {
	{ .name = "id",                 .type = DB_TYPE_SERIAL, .is_pk = true },
	{ .name = "timestamp",          .type = DB_TYPE_TIMESTAMP },
	{ .name = "type",		.type = DB_TYPE_TEXT },
	{ .name = "subtype",		.type = DB_TYPE_TEXT },
	{ .name = "instance",		.type = DB_TYPE_INT32 },
	{ .name = "socket_num",		.type = DB_TYPE_INT32 },
	{ .name = "ce_report_reg",	.type = DB_TYPE_INT32 },
	{ .name = "ce_location",	.type = DB_TYPE_INT32 },
	{ .name = "ce_addr",		.type = DB_TYPE_INT32 },
	{ .name = "ue_report_reg",	.type = DB_TYPE_INT32 },
	{ .name = "ue_location",	.type = DB_TYPE_INT32 },
	{ .name = "ue_addr",		.type = DB_TYPE_INT32 },
	{ .name = "reserved1",		.type = DB_TYPE_INT32 },
	{ .name = "reserved2",		.type = DB_TYPE_INT64 },
	{ .name = "reserved3",		.type = DB_TYPE_INT64 },
};

static const struct db_table_descriptor amp_payload2_event_tab = {
	.name = "amp_payload2_event",
	.fields = amp_payload2_event_fields,
	.num_fields = ARRAY_SIZE(amp_payload2_event_fields),
};

/*key pair definition for ampere specific error payload type 3*/
static const struct db_fields amp_payload3_event_fields[] = {
	{ .name = "id",                 .type = DB_TYPE_SERIAL, .is_pk = true },
	{ .name = "timestamp",          .type = DB_TYPE_TIMESTAMP },
	{ .name = "type",		.type = DB_TYPE_TEXT },
	{ .name = "subtype",		.type = DB_TYPE_TEXT },
	{ .name = "instance",		.type = DB_TYPE_INT32 },
	{ .name = "socket_num",		.type = DB_TYPE_INT32 },
	{ .name = "fw_spec_data0",	.type = DB_TYPE_INT64 },
	{ .name = "fw_spec_data1",	.type = DB_TYPE_INT64 },
	{ .name = "fw_spec_data2",	.type = DB_TYPE_INT64 },
	{ .name = "fw_spec_data3",	.type = DB_TYPE_INT64 },
	{ .name = "fw_spec_data4",	.type = DB_TYPE_INT64 },
	{ .name = "fw_spec_data5",	.type = DB_TYPE_INT64 },
};

static const struct db_table_descriptor amp_payload3_event_tab = {
	.name = "amp_payload3_event",
	.fields = amp_payload3_event_fields,
	.num_fields = ARRAY_SIZE(amp_payload3_event_fields),
};

static struct db_desc_and_stmt amp_payload0_event_db = {
	.desc = &amp_payload0_event_tab,
};

static struct db_desc_and_stmt amp_payload1_event_db = {
	.desc = &amp_payload1_event_tab,
};

static struct db_desc_and_stmt amp_payload2_event_db = {
	.desc = &amp_payload2_event_tab,
};

static struct db_desc_and_stmt amp_payload3_event_db = {
	.desc = &amp_payload3_event_tab,
};

static struct db_desc_and_stmt * const amp_payload_event_dbs[] = {
	&amp_payload0_event_db,
	&amp_payload1_event_db,
	&amp_payload2_event_db,
	&amp_payload3_event_db,
};

/* save all Ampere Specific Error Payload type 0 to database */
static void record_amp_payload0_err(struct ras_stmt *stmt, const char *type_str,
				    const char *subtype_str,
				    const struct amp_payload0_type_sec *err)
{
	db_bind(&amp_payload0_event_tab, stmt, AMP_PAYLOAD0_FIELD_TYPE,
		(uint64_t)type_str, -1);
	db_bind(&amp_payload0_event_tab, stmt, AMP_PAYLOAD0_FIELD_SUB_TYPE,
		(uint64_t)subtype_str, -1);

	db_bind(&amp_payload0_event_tab, stmt, AMP_PAYLOAD0_FIELD_INS,
		INSTANCE(err->instance), -1);

	db_bind(&amp_payload0_event_tab, stmt, AMP_PAYLOAD0_FIELD_SOCKET_NUM,
		SOCKET_NUM(err->instance), -1);
	db_bind(&amp_payload0_event_tab, stmt, AMP_PAYLOAD0_FIELD_STATUS_REG,
		err->err_status, -1);
	db_bind(&amp_payload0_event_tab, stmt, AMP_PAYLOAD0_FIELD_ADDR_REG,
		err->err_addr, -1);
	db_bind(&amp_payload0_event_tab, stmt, AMP_PAYLOAD0_FIELD_MISC0,
		err->err_misc_0, -1);
	db_bind(&amp_payload0_event_tab, stmt, AMP_PAYLOAD0_FIELD_MISC1,
		err->err_misc_1, -1);
	db_bind(&amp_payload0_event_tab, stmt, AMP_PAYLOAD0_FIELD_MISC2,
		err->err_misc_2, -1);
	db_bind(&amp_payload0_event_tab, stmt, AMP_PAYLOAD0_FIELD_MISC3,
		err->err_misc_3, -1);
	db_eval_stmt(stmt, "amp_payload0_event_tab");
}

/* save all Ampere Specific Error Payload type 1 to database */
static void record_amp_payload1_err(struct ras_stmt *stmt, const char *type_str,
				    const char *subtype_str,
				    const struct amp_payload1_type_sec *err)
{
	db_bind(&amp_payload1_event_tab, stmt, AMP_PAYLOAD1_FIELD_TYPE,
		(uint64_t)type_str, -1);
	db_bind(&amp_payload1_event_tab, stmt, AMP_PAYLOAD1_FIELD_SUB_TYPE,
		(uint64_t)subtype_str, -1);
	db_bind(&amp_payload1_event_tab, stmt, AMP_PAYLOAD1_FIELD_INS,
		INSTANCE(err->instance), -1);
	db_bind(&amp_payload1_event_tab, stmt, AMP_PAYLOAD1_FIELD_SOCKET_NUM,
		SOCKET_NUM(err->instance), -1);
	db_bind(&amp_payload1_event_tab, stmt,
		AMP_PAYLOAD1_FIELD_UNCORE_ERR_STATUS, err->uncore_status, -1);
	db_bind(&amp_payload1_event_tab, stmt,
		AMP_PAYLOAD1_FIELD_UNCORE_ERR_MASK, err->uncore_mask, -1);
	db_bind(&amp_payload1_event_tab, stmt,
		AMP_PAYLOAD1_FIELD_UNCORE_ERR_SEV, err->uncore_sev, -1);
	db_bind(&amp_payload1_event_tab, stmt,
		AMP_PAYLOAD1_FIELD_CORE_ERR_STATUS, err->core_status, -1);
	db_bind(&amp_payload1_event_tab, stmt, AMP_PAYLOAD1_FIELD_CORE_ERR_MASK,
		err->core_mask, -1);
	db_bind(&amp_payload1_event_tab, stmt, AMP_PAYLOAD1_FIELD_ROOT_ERR_CMD,
		err->root_err_cmd, -1);
	db_bind(&amp_payload1_event_tab, stmt,
		AMP_PAYLOAD1_FIELD_ROOT_ERR_STATUS, err->root_status, -1);
	db_bind(&amp_payload1_event_tab, stmt, AMP_PAYLOAD1_FIELD_SRC_ID,
		err->src_id, -1);
	db_bind(&amp_payload1_event_tab, stmt, AMP_PAYLOAD1_FIELD_RESERVED1,
		err->reserved1, -1);
	db_bind(&amp_payload1_event_tab, stmt, AMP_PAYLOAD1_FIELD_RESERVED2,
		err->reserved2, -1);
	db_eval_stmt(stmt, "amp_payload1_event_tab");
}

/* save all Ampere Specific Error Payload type 2 to database */
static void record_amp_payload2_err(struct ras_stmt *stmt, const char *type_str,
				    const char *subtype_str,
				    const struct amp_payload2_type_sec *err)
{
	db_bind(&amp_payload2_event_tab, stmt, AMP_PAYLOAD2_FIELD_TYPE,
		(uint64_t)type_str, -1);
	db_bind(&amp_payload2_event_tab, stmt, AMP_PAYLOAD2_FIELD_SUB_TYPE,
		(uint64_t)subtype_str, -1);
	db_bind(&amp_payload2_event_tab, stmt, AMP_PAYLOAD2_FIELD_INS,
		INSTANCE(err->instance), -1);
	db_bind(&amp_payload2_event_tab, stmt, AMP_PAYLOAD2_FIELD_SOCKET_NUM,
		SOCKET_NUM(err->instance), -1);
	db_bind(&amp_payload2_event_tab, stmt, AMP_PAYLOAD2_FIELD_CE_REPORT_REG,
		err->ce_register, -1);
	db_bind(&amp_payload2_event_tab, stmt, AMP_PAYLOAD2_FIELD_CE_LOACATION,
		err->ce_location, -1);
	db_bind(&amp_payload2_event_tab, stmt, AMP_PAYLOAD2_FIELD_CE_ADDR,
		err->ce_addr, -1);
	db_bind(&amp_payload2_event_tab, stmt, AMP_PAYLOAD2_FIELD_UE_REPORT_REG,
		err->ue_register, -1);
	db_bind(&amp_payload2_event_tab, stmt, AMP_PAYLOAD2_FIELD_UE_LOCATION,
		err->ue_location, -1);
	db_bind(&amp_payload2_event_tab, stmt, AMP_PAYLOAD2_FIELD_UE_ADDR,
		err->ue_addr, -1);
	db_bind(&amp_payload2_event_tab, stmt, AMP_PAYLOAD2_FIELD_RESERVED1,
		err->reserved1, -1);
	db_bind(&amp_payload2_event_tab, stmt, AMP_PAYLOAD2_FIELD_RESERVED2,
		err->reserved2, -1);
	db_bind(&amp_payload2_event_tab, stmt, AMP_PAYLOAD2_FIELD_RESERVED3,
		err->reserved3, -1);
	db_eval_stmt(stmt, "amp_payload2_event_tab");
}

/* save all Ampere Specific Error Payload type 3 to database */
static void record_amp_payload3_err(struct ras_stmt *stmt, const char *type_str,
				    const char *subtype_str,
				    const struct amp_payload3_type_sec *err)
{
	db_bind(&amp_payload3_event_tab, stmt, AMP_PAYLOAD3_FIELD_TYPE,
		(uint64_t)type_str, -1);
	db_bind(&amp_payload3_event_tab, stmt, AMP_PAYLOAD3_FIELD_SUB_TYPE,
		(uint64_t)subtype_str, -1);
	db_bind(&amp_payload3_event_tab, stmt, AMP_PAYLOAD3_FIELD_INS,
		INSTANCE(err->instance), -1);
	db_bind(&amp_payload3_event_tab, stmt, AMP_PAYLOAD3_FIELD_SOCKET_NUM,
		SOCKET_NUM(err->instance), -1);
	db_bind(&amp_payload3_event_tab, stmt, AMP_PAYLOAD3_FIELD_FW_SPEC_DATA0,
		err->fw_speci_data0, -1);
	db_bind(&amp_payload3_event_tab, stmt, AMP_PAYLOAD3_FIELD_FW_SPEC_DATA1,
		err->fw_speci_data1, -1);
	db_bind(&amp_payload3_event_tab, stmt, AMP_PAYLOAD3_FIELD_FW_SPEC_DATA2,
		err->fw_speci_data2, -1);
	db_bind(&amp_payload3_event_tab, stmt, AMP_PAYLOAD3_FIELD_FW_SPEC_DATA3,
		err->fw_speci_data3, -1);
	db_bind(&amp_payload3_event_tab, stmt, AMP_PAYLOAD3_FIELD_FW_SPEC_DATA4,
		err->fw_speci_data4, -1);
	db_bind(&amp_payload3_event_tab, stmt, AMP_PAYLOAD3_FIELD_FW_SPEC_DATA5,
		err->fw_speci_data5, -1);
	db_eval_stmt(stmt, "amp_payload3_event_tab");
}

/*
 * decode ampere specific error payload type 0, the CPU's data is save
 * to SQL DB by ras-arm-handler, others are saved by this function.
 */
void decode_amp_payload0_err_regs(struct ras_ns_ev_decoder *ev_decoder,
				  struct trace_seq *s,
				  const struct amp_payload0_type_sec *err)
{
	char buf[AMP_PAYLOAD0_BUF_LEN];
	char *p = buf;
	char *end = buf + AMP_PAYLOAD0_BUF_LEN;
	int i = 0, core_num = 0;
	const char *subtype_str;

	const char *type_str = oem_type_name(amp_payload_error_type,
					    TYPE(err->type));

	if (TYPE(err->type) == AMP_RAS_TYPE_SMMU)
		subtype_str = err_smmu_sub_type(err->subtype);
	else
		subtype_str  = oem_subtype_name(amp_payload_error_type,
						TYPE(err->type), err->subtype);

	//display error type
	p += snprintf(p, end - p, " %s", disp_payload1_err_reg_name[i++]);
	p += snprintf(p, end - p, " %s\n", type_str);

	//display error subtype
	p += snprintf(p, end - p, " %s", disp_payload1_err_reg_name[i++]);
	p += snprintf(p, end - p, " %s\n", subtype_str);

	//display error instance
	p += snprintf(p, end - p, " %s", disp_payload1_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%x\n", INSTANCE(err->instance));

	//display socket number
	if (!TYPE(err->type) && (err->subtype == 0x01 || err->subtype == 0x02)) {
		core_num = INSTANCE(err->instance) * 2 + err->subtype - 1;
		p += snprintf(p, end - p, " %s",
			      disp_payload1_err_reg_name[i++]);
		p += snprintf(p, end - p, " %d, Core Number is:%d\n",
		     SOCKET_NUM(err->instance), core_num);
	} else {
		p += snprintf(p, end - p, " %s",
		disp_payload1_err_reg_name[i++]);
		p += snprintf(p, end - p, " %d\n", SOCKET_NUM(err->instance));
	}

	//display status register
	p += snprintf(p, end - p, " %s", disp_payload0_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%x\n", err->err_status);

	//display address register
	p += snprintf(p, end - p, " %s", disp_payload0_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%llx\n",
		     (unsigned long long)err->err_addr);

	//display MISC0
	p += snprintf(p, end - p, " %s", disp_payload0_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%llx\n",
		     (unsigned long long)err->err_misc_0);

	//display MISC1
	p += snprintf(p, end - p, " %s", disp_payload0_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%llx\n",
		     (unsigned long long)err->err_misc_1);

	//display MISC2
	p += snprintf(p, end - p, " %s", disp_payload0_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%llx\n",
		     (unsigned long long)err->err_misc_2);

	//display MISC3
	p += snprintf(p, end - p, " %s", disp_payload0_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%llx\n",
		     (unsigned long long)err->err_misc_3);

	if (p > buf && p < end) {
		p--;
		*p = '\0';
	}

	record_amp_payload0_err(amp_payload0_event_db.stmt, type_str,
				subtype_str, err);
	i = 0;
	p = NULL;
	end = NULL;
	trace_seq_printf(s, "%s\n", buf);
}

/* decode ampere specific error payload type 1 and save to database */
static void decode_amp_payload1_err_regs(struct ras_ns_ev_decoder *ev_decoder,
					 struct trace_seq *s,
					 const struct amp_payload1_type_sec *err)
{
	char buf[AMP_PAYLOAD0_BUF_LEN];
	char *p = buf;
	char *end = buf + AMP_PAYLOAD0_BUF_LEN;
	int i = 0;

	const char *type_str = oem_type_name(amp_payload_error_type,
					     TYPE(err->type));
	const char *subtype_str = oem_subtype_name(amp_payload_error_type,
						TYPE(err->type), err->subtype);

	//display error type
	p += snprintf(p, end - p, " %s", disp_payload1_err_reg_name[i++]);
	p += snprintf(p, end - p, " %s\n", type_str);

	//display error subtype
	p += snprintf(p, end - p, " %s", disp_payload1_err_reg_name[i++]);
	p += snprintf(p, end - p, " %s", subtype_str);

	//display error instance
	p += snprintf(p, end - p, "\n%s", disp_payload1_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%x\n", INSTANCE(err->instance));

	//display socket number
	p += snprintf(p, end - p, " %s", disp_payload1_err_reg_name[i++]);
	p += snprintf(p, end - p, " %d\n", SOCKET_NUM(err->instance));

	//display AER_UNCORR_ERR_STATUS
	p += snprintf(p, end - p, " %s", disp_payload1_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%x\n", err->uncore_status);

	//display AER_UNCORR_ERR_MASK
	p += snprintf(p, end - p, " %s", disp_payload1_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%x\n", err->uncore_mask);

	//display AER_UNCORR_ERR_SEV
	p += snprintf(p, end - p, " %s", disp_payload1_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%x\n", err->uncore_sev);

	//display AER_CORR_ERR_STATUS
	p += snprintf(p, end - p, " %s", disp_payload1_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%x\n", err->core_status);

	//display AER_CORR_ERR_MASK
	p += snprintf(p, end - p, " %s", disp_payload1_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%x\n", err->core_mask);

	//display AER_ROOT_ERR_CMD
	p += snprintf(p, end - p, " %s", disp_payload1_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%x\n", err->root_err_cmd);

	//display AER_ROOT_ERR_STATUS
	p += snprintf(p, end - p, " %s", disp_payload1_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%x\n", err->root_status);

	//display AER_ERR_SRC_ID
	p += snprintf(p, end - p, " %s", disp_payload1_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%x\n", err->src_id);

	//display Reserved
	p += snprintf(p, end - p, " %s", disp_payload1_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%x\n", err->reserved1);

	//display Reserved
	p += snprintf(p, end - p, " %s", disp_payload1_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%llx\n",
		     (unsigned long long)err->reserved2);

	if (p > buf && p < end) {
		p--;
		*p = '\0';
	}

	record_amp_payload1_err(amp_payload1_event_db.stmt, type_str,
				subtype_str, err);
	i = 0;
	p = NULL;
	end = NULL;
	trace_seq_printf(s, "%s\n", buf);
}

/* decode ampere specific error payload type 2 and save to database */
static void decode_amp_payload2_err_regs(struct ras_ns_ev_decoder *ev_decoder,
					 struct trace_seq *s,
					 const struct amp_payload2_type_sec *err)
{
	char buf[AMP_PAYLOAD0_BUF_LEN];
	char *p = buf;
	char *end = buf + AMP_PAYLOAD0_BUF_LEN;
	int i = 0;
	const char *subtype_str;

	const char *type_str = oem_type_name(amp_payload_error_type,
					     TYPE(err->type));

	if (TYPE(err->type) == AMP_RAS_TYPE_PCIE_RASDP)
		subtype_str = err_peci_rasdp_sub_type(err->subtype);
	else
		subtype_str  = oem_subtype_name(amp_payload_error_type,
						TYPE(err->type), err->subtype);
	//display error type
	p += snprintf(p, end - p, " %s", disp_payload2_err_reg_name[i++]);
	p += snprintf(p, end - p, " %s\n", type_str);

	//display error subtype
	p += snprintf(p, end - p, " %s", disp_payload2_err_reg_name[i++]);
	p += snprintf(p, end - p, " %s\n", subtype_str);

	//display error instance
	p += snprintf(p, end - p, " %s", disp_payload2_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%x\n", INSTANCE(err->instance));

	//display socket number
	p += snprintf(p, end - p, " %s", disp_payload2_err_reg_name[i++]);
	p += snprintf(p, end - p, " %d\n", SOCKET_NUM(err->instance));

	//display CE Report Register
	p += snprintf(p, end - p, " %s", disp_payload2_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%x\n", err->ce_register);

	//display CE Location Register
	p += snprintf(p, end - p, " %s", disp_payload2_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%x\n", err->ce_location);

	//display CE Address
	p += snprintf(p, end - p, " %s", disp_payload2_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%x\n", err->ce_addr);

	//display UE Reprot Register
	p += snprintf(p, end - p, " %s", disp_payload2_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%x\n", err->ue_register);

	//display UE Location Register
	p += snprintf(p, end - p, " %s", disp_payload2_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%x\n", err->ue_location);

	//display UE Address
	p += snprintf(p, end - p, " %s", disp_payload2_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%x\n", err->ue_addr);

	//display Reserved
	p += snprintf(p, end - p, " %s", disp_payload2_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%x\n", err->reserved1);

	//display Reserved
	p += snprintf(p, end - p, " %s", disp_payload2_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%llx\n",
		      (unsigned long long)err->reserved2);

	//display Reserved
	p += snprintf(p, end - p, " %s", disp_payload2_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%llx\n",
		      (unsigned long long)err->reserved3);

	if (p > buf && p < end) {
		p--;
		*p = '\0';
	}

	record_amp_payload2_err(amp_payload2_event_db.stmt, type_str,
				subtype_str, err);
	i = 0;
	p = NULL;
	end = NULL;
	trace_seq_printf(s, "%s\n", buf);
}

/* decode ampere specific error payload type 3 and save to database */
static void decode_amp_payload3_err_regs(struct ras_ns_ev_decoder *ev_decoder,
					 struct trace_seq *s,
					 const struct amp_payload3_type_sec *err)
{
	char buf[AMP_PAYLOAD0_BUF_LEN];
	char *p = buf;
	char *end = buf + AMP_PAYLOAD0_BUF_LEN;
	int i = 0;

	const char *type_str = oem_type_name(amp_payload_error_type,
					     TYPE(err->type));
	const char *subtype_str = oem_subtype_name(amp_payload_error_type,
						 TYPE(err->type), err->subtype);

	//display error type
	p += snprintf(p, end - p, " %s", disp_payload3_err_reg_name[i++]);
	p += snprintf(p, end - p, " %s\n", type_str);

	//display error subtype
	p += snprintf(p, end - p, " %s", disp_payload3_err_reg_name[i++]);
	p += snprintf(p, end - p, " %s\n", subtype_str);

	//display error instance
	p += snprintf(p, end - p, " %s", disp_payload3_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%x\n", INSTANCE(err->instance));

	//display socket number
	p += snprintf(p, end - p, " %s", disp_payload3_err_reg_name[i++]);
	p += snprintf(p, end - p, " %d\n", SOCKET_NUM(err->instance));

	//display Firmware-Specific Data 0
	p += snprintf(p, end - p, " %s", disp_payload3_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%x\n", err->fw_speci_data0);

	//display Firmware-Specific Data 1
	p += snprintf(p, end - p, " %s", disp_payload3_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%llx\n",
		     (unsigned long long)err->fw_speci_data1);

	//display Firmware-Specific Data 2
	p += snprintf(p, end - p, " %s", disp_payload3_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%llx\n",
		     (unsigned long long)err->fw_speci_data2);

	//display Firmware-Specific Data 3
	p += snprintf(p, end - p, " %s", disp_payload3_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%llx\n",
		     (unsigned long long)err->fw_speci_data3);

	//display Firmware-Specific Data 4
	p += snprintf(p, end - p, " %s", disp_payload3_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%llx\n",
		     (unsigned long long)err->fw_speci_data4);

	//display Firmware-Specific Data 5
	p += snprintf(p, end - p, " %s", disp_payload3_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%llx\n",
		     (unsigned long long)err->fw_speci_data5);

	if (p > buf && p < end) {
		p--;
		*p = '\0';
	}

	record_amp_payload3_err(amp_payload3_event_db.stmt, type_str,
				subtype_str, err);
	i = 0;
	p = NULL;
	end = NULL;
	trace_seq_printf(s, "%s\n", buf);
}

/* error data decoding functions */
static int decode_amp_oem_type_error(struct ras_events *ras,
				     struct ras_ns_ev_decoder *ev_decoder,
				     struct trace_seq *s,
				     struct ras_non_standard_event *event)
{
	struct db_desc_and_stmt *db;
	int payload_type = PAYLOAD_TYPE(event->error[0]);
	int id = 0;

	switch (payload_type) {
	case PAYLOAD_TYPE_0:
		db = &amp_payload0_event_db;
		WARN_ONCE(ras->record_events && !db->stmt, ALL, LOG_WARNING,
			  "Can't insert into table %s: no statement\n",
			  db->desc->name);
		id = AMP_PAYLOAD0_FIELD_TIMESTAMP;
		break;
	case PAYLOAD_TYPE_1:
		db = &amp_payload1_event_db;
		WARN_ONCE(ras->record_events && !db->stmt, ALL, LOG_WARNING,
			  "Can't insert into table %s: no statement\n",
			  db->desc->name);
		id = AMP_PAYLOAD1_FIELD_TIMESTAMP;
		break;
	case PAYLOAD_TYPE_2:
		db = &amp_payload2_event_db;
		WARN_ONCE(ras->record_events && !db->stmt, ALL, LOG_WARNING,
			  "Can't insert into table %s: no statement\n",
			  db->desc->name);
		id = AMP_PAYLOAD2_FIELD_TIMESTAMP;
		break;
	case PAYLOAD_TYPE_3:
		db = &amp_payload3_event_db;
		WARN_ONCE(ras->record_events && !db->stmt, ALL, LOG_WARNING,
			  "Can't insert into table %s: no statement\n",
			  db->desc->name);
		id = AMP_PAYLOAD3_FIELD_TIMESTAMP;
		break;
	default:
		return -1;
	}

	db_bind(db->desc, db->stmt, id, (uint64_t)event->timestamp, -1);

	switch (payload_type) {
	case PAYLOAD_TYPE_0: {
		const struct amp_payload0_type_sec *err =
		    (struct amp_payload0_type_sec *)event->error;
		decode_amp_payload0_err_regs(ev_decoder, s, err);
		break;
	}
	case PAYLOAD_TYPE_1: {
		const struct amp_payload1_type_sec *err =
			(struct amp_payload1_type_sec *)event->error;
		decode_amp_payload1_err_regs(ev_decoder, s, err);
		break;
	}
	case PAYLOAD_TYPE_2: {
		const struct amp_payload2_type_sec *err =
			(struct amp_payload2_type_sec *)event->error;
		decode_amp_payload2_err_regs(ev_decoder, s, err);
		break;
	}
	case PAYLOAD_TYPE_3: {
		const struct amp_payload3_type_sec *err =
			(struct amp_payload3_type_sec *)event->error;
		decode_amp_payload3_err_regs(ev_decoder, s, err);
		break;
	}
	}

	return 0;
}

struct ras_ns_ev_decoder amp_ns_oem_decoder[] = {
	{
		.sec_type = "e8ed898d-df16-43cc-8ecc-54f060ef157f",
		.decode = decode_amp_oem_type_error,
	},
};

static int amp_init(struct ras_module_ctx *ctx)
{
	size_t i;
	int rc;

	for (i = 0; i < ARRAY_SIZE(amp_payload_event_dbs); i++) {
		rc = ras_db_table_register(ctx, amp_payload_event_dbs[i]);
		if (rc) {
			ras_db_table_unregister(ctx);
			return rc;
		}
	}

	rc = register_ns_ev_decoder(amp_ns_oem_decoder);
	if (rc)
		ras_db_table_unregister(ctx);

	return rc;
}

static void amp_cleanup(struct ras_module_ctx *ctx)
{
	unregister_ns_ev_decoder(amp_ns_oem_decoder);
	ras_db_table_unregister(ctx);
}

static const struct ras_module_entry amp_module = {
	.name = "non-standard-ampere",
	.level = SUB_EVENT_MODULE,
	.init = amp_init,
	.cleanup = amp_cleanup,
};

static void __attribute__((constructor)) amp_register(void)
{
	int rc = module_register(&amp_module);

	if (rc)
		log(TERM, LOG_ERR, "Failed to register Ampere module: %d\n", rc);
}

#ifdef HAVE_UNITTEST
struct db_table_descriptor_list ampere_table_descriptors(void)
{
	static const struct db_table_descriptor * const tables[] = {
		&amp_payload0_event_tab, &amp_payload1_event_tab,
		&amp_payload2_event_tab, &amp_payload3_event_tab,
	};

	return (struct db_table_descriptor_list) { tables, ARRAY_SIZE(tables) };
}
#endif
