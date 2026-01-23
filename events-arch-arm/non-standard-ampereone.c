// SPDX-License-Identifier: GPL-2.0-only

/*
 * Copyright (c) 2025, Ampere Computing LLC
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "ras-record.h"
#include "ras-logger.h"
#include "ras-report.h"
#include "ras-non-standard-handler.h"
#include "non-standard-ampereone.h"

#define HEX_WIDTH 16

static void display_hex_dump(char **p, char *end, uint8_t size, const uint8_t *byte_array,
							 const char *disp_reg_name);

static const char * const disp_ampereone_payload0_err_reg_name[] = {
	"Error Type:",
	"Error SubType:",
	"Error Instance:",
	"Processor Socket:",
	"ERRxFR:",
	"ERRxCTLR:",
	"Status:",
	"Address:",
	"MISC0:",
	"MISC1:",
	"MISC2:",
	"MISC3:",
};

static const char * const disp_ampereone_payload1_err_reg_name[] = {
	"Error Type:",
	"Error SubType:",
	"Error Instance:",
	"Processor Socket:",
	"AER_UE_ERR_STATUS:",
	"AER_CE_ERR_STATUS:",
	"EBUF_OVERFLOW:",
	"EBUF_UNDERRUN:",
	"DECODE_ERROR:",
	"RUNNING_DISPARITY_ERROR:",
	"SKP_OS_PARITY_ERROR_GEN3:",
	"SYNC_HEADER_ERROR:",
	"RX_VALID_DEASSERTION:",
	"CTL_SKP_OS_PARITY_ERROR_GEN4:",
	"FIRST_RETIMER_PARITY_ERROR_GEN4:",
	"SECOND_RETIMER_PARITY_ERROR_GEN4:",
	"MARGIN_CRC_AND_PARITY_ERROR_GEN4:",
	"RASDES_GROUP1_COUNTERS:",
	"RSVD0:",
	"RASDES_GROUP2_COUNTERS:",
	"EBUF_SKP_ADD:",
	"EBUF_SKP_DEL:",
	"RASDES_GROUP5_COUNTERS:",
	"RASDES_GROUP5_COUNTERS_CONTINUED:",
	"RSVD1:",
	"Si Dbg L1 Sts Ln 0:",
	"Si Dbg L1 Sts Ln 1:",
	"Si Dbg L1 Sts Ln 2:",
	"Si Dbg L1 Sts Ln 3:",
	"Si Dbg L1 Sts Ln 4:",
	"Si Dbg L1 Sts Ln 5:",
	"Si Dbg L1 Sts Ln 6:",
	"Si Dbg L1 Sts Ln 7:",
	"Si Dbg L1 Sts Ln 8:",
	"Si Dbg L1 Sts Ln 9:",
	"Si Dbg L1 Sts Ln 10:",
	"Si Dbg L1 Sts Ln 11:",
	"Si Dbg L1 Sts Ln 12:",
	"Si Dbg L1 Sts Ln 13:",
	"Si Dbg L1 Sts Ln 14:",
	"Si Dbg L1 Sts Ln 15:",
};

static const char * const disp_ampereone_payload2_err_reg_name[] = {
	"Error Type:",
	"Error SubType:",
	"Error Instance:",
	"Processor Socket:",
	"CORR_COUNT_REPORT:",
	"CORR_ERROR_LOCATION:",
	"RAM_ADDR_CORR:",
	"UNCORR_COUNT_REPORT:",
	"UNCORR_ERROR_LOCATION:",
	"RAM_ADDR_UNCORR:",
};

static const char * const disp_ampereone_payload3_err_reg_name[] = {
	"Error Type:",
	"Error SubType:",
	"Error Instance:",
	"Processor Socket:",
	"ECCxAddr:",
	"ECCxData:",
	"ECCxId:",
	"ECCxSynd:",
	"ECCxMceCnt:",
	"ECCxCtlr:",
	"ECCxErrSts:",
	"ECCxErrCnt:",
};

static const char * const disp_ampereone_payload4_err_reg_name[] = {
	"Error Type:",
	"Error SubType:",
	"Error Instance:",
	"Processor Socket:",
	"MCU CHI Addr:",
	"MCU CHI SrcId:",
	"MCU CHI TxnId:",
	"MCU CHI Type:",
	"MCU CHI LpId:",
	"MCU CHI Opcode:",
	"MCU CHI Tag:",
	"MCU CHI MPAM:"
};

static const char * const disp_ampereone_payload5_err_reg_name[] = {
	"Error Type:",
	"Error SubType:",
	"Error Instance:",
	"Processor Socket:",
};

static const char * const disp_ampereone_payload6_err_reg_name[] = {
	"Error Type:",
	"Error SubType:",
	"Error Instance:",
	"Processor Socket:",
	"Driver:",
	"Error Code:",
	"Error Msg Size:",
	"Error Msg:"
};

static const char * const err_ampereone_pe_sub_type[] = {
	"PE L2C (CE/DE)",
	"PE L2C (UER/UC)",
	"PE MMU DTLB (CE)",
	"PE MMU DTLB (UER)",
	"PE MMU ITLB (CE)",
	"PE MMU ITLB (UER)",
	"PE LSU (CE)",
	"PE LSU (UC)",
	"PE ICF (CE)",
	"PE ICF (UC)",
	"RESERVED SUBTYPE2",
	"PE IXU (UC)",
	"RESERVED SUBTYPE3",
	"PE FSU (UC)",
};

static const char * const err_ampereone_mcu_sub_type[] = {
	"MCU ECC (CE)",
	"MCU ECC RMW (CE)",
	"MCU ECC (UE)",
	"MCU ECC RMW (UE)",
	"MCU CHI Request Error",
	"MCU CHI Unauthorized Error",
	"MCU CHI Write Data Error",
	"MCU CHI Write Data Corrupted Error",
	"MCU CHI Write Data Parity Error",
	"MCU Parity Error",
	"MCU Scrub ECC CE",
	"MCU CHI Out Of Range Error",
	"MCU SRAM Error",
};

static const char * const err_ampereone_cmn_sub_type[] = {
	"CMN Cross Point",
	"CMN Home Node(IO)",
	"CMN Home Node(Memory)",
	"CMN CCIX Node",
};

static const char * const err_ampereone_2p_link_aer_sub_type[] = {
	"ERR0",
};

static const char * const err_ampereone_2p_link_ali_sub_type[] = {
	"ERR0",
	"ERR1",
	"ERR2",
	"ERR3",
};

static const char * const err_ampereone_gic_sub_type[] = {
	"ERR0",	"ERR1",	"ERR2",
	"ERR3",	"ERR4",	"ERR5",
	"ERR6",	"ERR7",	"ERR8",
	"ERR9",	"ERR10",	"ERR11",
	"ERR12",	"ERR13",	"ERR14",
	"ERR15",	"ERR16",	"ERR17",
	"ERR18",	"ERR19",	"ERR20",
	"ERR21",	"ERR22",	"ERR23",
	"ERR24",	"ERR25",	"ERR26",
	"ERR27",	"ERR28",	"ERR29",
	"ERR30",	"ERR31",	"ERR32",
	"ERR33",	"ERR34",
};

static const char * const err_ampereone_smmu_sub_type[] = {
	"TCU",
	"TBU0_RD",
	"TBU1_WR",
};

static const char * const err_ampereone_pcie_aer_sub_type[] = {
	"RP",
	"DEVICE"
};

static const char * const err_ampereone_pcie_hostbridge_sub_type[] = {
	"ERR0",
};

static const char * const err_ampereone_pcie_rasdp_sub_type[] = {
	"ERR0",
};

static const char * const err_ampereone_ocm_sub_type[] = {
	"ERR0",
	"ERR1",
	"ERR2",
};

static const char * const err_ampereone_mpro_sub_type[] = {
	"ERR0",
	"ERR1",
	"ERR2",
	"ERR3",
	"ERR4",
	"ERR5",
};

static const char * const err_ampereone_secpro_sub_type[] = {
	"ERR0",
	"ERR1",
	"ERR2",
	"ERR3",
};

static char *sqlite3_table_list[] = {
	"ampereone_payload0_event_tab",
	"ampereone_payload1_event_tab",
	"ampereone_payload2_event_tab",
	"ampereone_payload3_event_tab",
	"ampereone_payload4_event_tab",
	"ampereone_payload5_event_tab",
	"ampereone_payload6_event_tab",
};
struct ampereone_ras_type_info {
	int id;
	const char *name;
	const char * const *sub;
	int sub_num;
};

static const struct ampereone_ras_type_info ampereone_payload_error_type[] = {
	{
		.id = AMPEREONE_RAS_TYPE_CPU,
		.name = "PE",
		.sub = err_ampereone_pe_sub_type,
		.sub_num = ARRAY_SIZE(err_ampereone_pe_sub_type),
	},
	{
		.id = AMPEREONE_RAS_TYPE_MCU,
		.name = "MCU",
		.sub = err_ampereone_mcu_sub_type,
		.sub_num = ARRAY_SIZE(err_ampereone_mcu_sub_type),
	},
	{
		.id = AMPEREONE_RAS_TYPE_CMN,
		.name = "CMN",
		.sub = err_ampereone_cmn_sub_type,
		.sub_num = ARRAY_SIZE(err_ampereone_cmn_sub_type),
	},
	{
		.id = AMPEREONE_RAS_TYPE_2P_LINK_AER,
		.name = "2P-Link-AER",
		.sub = err_ampereone_2p_link_aer_sub_type,
		.sub_num = ARRAY_SIZE(err_ampereone_2p_link_aer_sub_type),
	},
	{
		.id = AMPEREONE_RAS_TYPE_2P_LINK_ALI,
		.name = "2P-Link-ALI",
		.sub = err_ampereone_2p_link_ali_sub_type,
		.sub_num = ARRAY_SIZE(err_ampereone_2p_link_ali_sub_type),
	},
	{
		.id = AMPEREONE_RAS_TYPE_GIC,
		.name = "GIC",
		.sub = err_ampereone_gic_sub_type,
		.sub_num = ARRAY_SIZE(err_ampereone_gic_sub_type),
	},
	{
		.id = AMPEREONE_RAS_TYPE_SMMU,
		.name = "SMMU",
		.sub = err_ampereone_smmu_sub_type,
		.sub_num = ARRAY_SIZE(err_ampereone_smmu_sub_type),
	},
	{
		.id = AMPEREONE_RAS_TYPE_PCIE_AER,
		.name = "PCIe AER",
		.sub = err_ampereone_pcie_aer_sub_type,
		.sub_num = ARRAY_SIZE(err_ampereone_pcie_aer_sub_type),
	},
	{
		.id = AMPEREONE_RAS_TYPE_PCIE_HB,
		.name = "PCIe AER",
		.sub = err_ampereone_pcie_hostbridge_sub_type,
		.sub_num = ARRAY_SIZE(err_ampereone_pcie_hostbridge_sub_type),
	},
	{
		.id = AMPEREONE_RAS_TYPE_PCIE_RASDP,
		.name = "PCIe RASDP",
		.sub = err_ampereone_pcie_rasdp_sub_type,
		.sub_num = ARRAY_SIZE(err_ampereone_pcie_rasdp_sub_type),
	},
	{
		.id = AMPEREONE_RAS_TYPE_OCM,
		.name = "OCM",
		.sub = err_ampereone_ocm_sub_type,
		.sub_num = ARRAY_SIZE(err_ampereone_ocm_sub_type),
	},
	{
		.id = AMPEREONE_RAS_TYPE_MPRO,
		.name = "MPRO",
		.sub = err_ampereone_mpro_sub_type,
		.sub_num = ARRAY_SIZE(err_ampereone_mpro_sub_type),
	},
	{
		.id = AMPEREONE_RAS_TYPE_SECPRO,
		.name = "SECPRO",
		.sub = err_ampereone_secpro_sub_type,
		.sub_num = ARRAY_SIZE(err_ampereone_secpro_sub_type),
	},
	{
		.id = AMPEREONE_RAS_TYPE_INTERNAL_SOC,
		.name = "Internal SoC Error",
		.sub = NULL,
		.sub_num = 0,
	},
	{
		.id = AMPEREONE_RAS_TYPE_MPRO_FW,
		.name = "Mpro Firmware",
		.sub = NULL,
		.sub_num = 0,
	},
	{
		.id = AMPEREONE_RAS_TYPE_SECPRO_FW,
		.name = "SECpro Firmware",
		.sub = NULL,
		.sub_num = 0,
	},
	{}
};

void display_hex_dump(char **p, char *end, uint8_t size, const uint8_t *byte_array,
					  const char *disp_reg_name)
{
	for (uint8_t c = 0; c < size; c++) {

		if (c == 0)
			*p += snprintf(*p, end - *p, " %s\n", disp_reg_name);
		if (c % HEX_WIDTH == 0) {
			*p += snprintf(*p, end - *p, "\t0x%08x: ",
				(uint8_t)c);
		}
		*p += snprintf(*p, end - *p, "%02x ",
			(uint8_t)*(byte_array + c));
		if (c % HEX_WIDTH == HEX_WIDTH - 1)
			*p += snprintf(*p, end - *p, "\n");
	}
}

/*get the error type name*/
static const char *ampereone_type_name(const struct ampereone_ras_type_info *info,
				uint8_t type_id)
{
	const struct ampereone_ras_type_info *type = &info[0];

	for (; type->name; type++) {
		if (type->id != type_id)
			continue;
		return type->name;
	}
	return "unknown";
}

/*get the error subtype*/
static const char *ampereone_subtype_name(const struct ampereone_ras_type_info *info,
				    uint8_t type_id, uint8_t sub_type_id)
{
	const struct ampereone_ras_type_info *type = &info[0];

	for (; type->name; type++) {
		const char * const *submodule = type->sub;

		if (type->id != type_id)
			continue;
		if (type->sub == NULL)
			return type->name;
		if (sub_type_id >= type->sub_num)
			return "unknown";
		return submodule[sub_type_id];
	}
	return "unknown";
}

#ifdef HAVE_SQLITE3
/*key pair definition for ampere specific error payload type 0*/
static const struct db_fields ampereone_payload0_event_fields[] = {
	{ .name = "id",			.type = "INTEGER PRIMARY KEY" },
	{ .name = "timestamp",		.type = "TEXT" },
	{ .name = "type_id",		.type = "TEXT" },
	{ .name = "subtype_id",		.type = "TEXT" },
	{ .name = "instance",		.type = "INTEGER" },
	{ .name = "socket_num",		.type = "INTEGER" },
	{ .name = "ERRxFR",		.type = "INTEGER" },
	{ .name = "ERRxCTLR",		.type = "INTEGER" },
	{ .name = "ERRxSTATUS",		.type = "INTEGER" },
	{ .name = "ERRxADDR",		.type = "INTEGER" },
	{ .name = "ERRxMISC0",		.type = "INTEGER" },
	{ .name = "ERRxMISC1",		.type = "INTEGER" },
	{ .name = "ERRxMISC2",		.type = "INTEGER" },
	{ .name = "ERRxMISC3",		.type = "INTEGER" },
};

static const struct db_table_descriptor ampereone_payload0_event_tab = {
	.name = "ampereone_payload0_event",
	.fields = ampereone_payload0_event_fields,
	.num_fields = ARRAY_SIZE(ampereone_payload0_event_fields),
};

/*key pair definition for ampere specific error payload type 1*/
static const struct db_fields ampereone_payload1_event_fields[] = {
	{ .name = "id",					.type = "INTEGER PRIMARY KEY" },
	{ .name = "timestamp",				.type = "TEXT" },
	{ .name = "type_id",				.type = "TEXT" },
	{ .name = "subtype_id",				.type = "TEXT" },
	{ .name = "instance",				.type = "INTEGER" },
	{ .name = "socket_num",				.type = "INTEGER" },
	{ .name = "AER_CORR_ERR_STATUS",		.type = "INTEGER" },
	{ .name = "AER_UNCORR_ERR_STATUS",		.type = "INTEGER" },
	{ .name = "EBUF_OVERFLOW",			.type = "INTEGER" },
	{ .name = "EBUF_UNDERRUN",			.type = "INTEGER" },
	{ .name = "DECODE_ERR",			.type = "INTEGER" },
	{ .name = "RUNNING_DISPARITY_ERR",		.type = "INTEGER" },
	{ .name = "SKP_OS_PARITY_ERR_G3",		.type = "INTEGER" },
	{ .name = "SYNC_HEADER_ERR",			.type = "INTEGER" },
	{ .name = "RX_VALID_DEASSERTION",		.type = "INTEGER" },
	{ .name = "CTL_SKP_OS_PARITY_ERR_G4",	.type = "INTEGER" },
	{ .name = "FIRST_RETIMER_PARITY_ERR_G4",	.type = "INTEGER" },
	{ .name = "SECOND_RETIMER_PARITY_ERR_G4",	.type = "INTEGER" },
	{ .name = "MARGIN_CRC_AND_PARTIY_ERR_G4",	.type = "INTEGER" },
	{ .name = "RASDES_GRP1_COUNTERS",		.type = "INTEGER" },
	{ .name = "RSVD0",				.type = "INTEGER" },
	{ .name = "RASDES_GRP2_COUNTERS",		.type = "INTEGER" },
	{ .name = "EBUF_SKP_ADD",			.type = "INTEGER" },
	{ .name = "EBUF_SKP_DEL",			.type = "INTEGER" },
	{ .name = "RASDES_GRP5_CNTRS_0",		.type = "INTEGER" },
	{ .name = "RASDES_GRP5_CNTRS_1",	.type = "INTEGER" },
	{ .name = "RSVD1",				.type = "INTEGER" },
	{ .name = "L0",	.type = "INTEGER" },
	{ .name = "L1",	.type = "INTEGER" },
	{ .name = "L2",	.type = "INTEGER" },
	{ .name = "L3",	.type = "INTEGER" },
	{ .name = "L4",	.type = "INTEGER" },
	{ .name = "L5",	.type = "INTEGER" },
	{ .name = "L6",	.type = "INTEGER" },
	{ .name = "L7",	.type = "INTEGER" },
	{ .name = "L8",	.type = "INTEGER" },
	{ .name = "L9",	.type = "INTEGER" },
	{ .name = "L10",	.type = "INTEGER" },
	{ .name = "L11",	.type = "INTEGER" },
	{ .name = "L12",	.type = "INTEGER" },
	{ .name = "L13",	.type = "INTEGER" },
	{ .name = "L14",	.type = "INTEGER" },
	{ .name = "L15",	.type = "INTEGER" },
};

static const struct db_table_descriptor ampereone_payload1_event_tab = {
	.name = "ampereone_payload1_event",
	.fields = ampereone_payload1_event_fields,
	.num_fields = ARRAY_SIZE(ampereone_payload1_event_fields),
};

/*key pair definition for ampere specific error payload type 2*/
static const struct db_fields ampereone_payload2_event_fields[] = {
	{ .name = "id",				.type = "INTEGER PRIMARY KEY" },
	{ .name = "timestamp",			.type = "TEXT" },
	{ .name = "type_id",			.type = "TEXT" },
	{ .name = "subtype_id",			.type = "TEXT" },
	{ .name = "instance",			.type = "INTEGER" },
	{ .name = "socket_num",			.type = "INTEGER" },
	{ .name = "CORR_COUNT_REPORT",		.type = "INTEGER" },
	{ .name = "CORR_ERROR_LOCATION",	.type = "INTEGER" },
	{ .name = "RAM_ADDR_CORR",		.type = "INTEGER" },
	{ .name = "UNCORR_ERROR_LOCATION",	.type = "INTEGER" },
	{ .name = "RAM_ADDR_UNCORR",		.type = "INTEGER" },
};

static const struct db_table_descriptor ampereone_payload2_event_tab = {
	.name = "ampereone_payload2_event",
	.fields = ampereone_payload2_event_fields,
	.num_fields = ARRAY_SIZE(ampereone_payload2_event_fields),
};

/*key pair definition for ampere specific error payload type 3*/
static const struct db_fields ampereone_payload3_event_fields[] = {
	{ .name = "id",			.type = "INTEGER PRIMARY KEY" },
	{ .name = "timestamp",		.type = "TEXT" },
	{ .name = "type_id",		.type = "TEXT" },
	{ .name = "subtype_id",		.type = "TEXT" },
	{ .name = "instance",		.type = "INTEGER" },
	{ .name = "socket_num",		.type = "INTEGER" },
	{ .name = "ECC_ADDRESS",	.type = "INTEGER" },
	{ .name = "ECC_DATA",		.type = "INTEGER" },
	{ .name = "ECC_SRC_ID",		.type = "INTEGER" },
	{ .name = "ECC_SYND",		.type = "INTEGER" },
	{ .name = "ECC_MCE_CNT",	.type = "INTEGER" },
	{ .name = "ECC_CTLR",		.type = "INTEGER" },
	{ .name = "ECC_ERR_STS",	.type = "INTEGER" },
	{ .name = "ECC_ERR_CNT",	.type = "INTEGER" },
};

static const struct db_table_descriptor ampereone_payload3_event_tab = {
	.name = "ampereone_payload3_event",
	.fields = ampereone_payload3_event_fields,
	.num_fields = ARRAY_SIZE(ampereone_payload3_event_fields),
};

/*key pair definition for ampere specific error payload type 4*/
static const struct db_fields ampereone_payload4_event_fields[] = {
	{ .name = "id",		.type = "INTEGER PRIMARY KEY" },
	{ .name = "timestamp",	.type = "TEXT" },
	{ .name = "type_id",	.type = "TEXT" },
	{ .name = "subtype_id",	.type = "TEXT" },
	{ .name = "instance",	.type = "INTEGER" },
	{ .name = "socket_num",	.type = "INTEGER" },
	{ .name = "ADDRESS",	.type = "INTEGER" },
	{ .name = "SRC_ID",	.type = "INTEGER" },
	{ .name = "TXNID",	.type = "INTEGER" },
	{ .name = "TYPE",	.type = "INTEGER" },
	{ .name = "LPID",	.type = "INTEGER" },
	{ .name = "OPCODE",	.type = "INTEGER" },
	{ .name = "TAG",	.type = "INTEGER" },
	{ .name = "MPAM",	.type = "INTEGER" },
};

static const struct db_table_descriptor ampereone_payload4_event_tab = {
	.name = "ampereone_payload4_event",
	.fields = ampereone_payload4_event_fields,
	.num_fields = ARRAY_SIZE(ampereone_payload4_event_fields),
};

/*key pair definition for ampere specific error payload type 5*/
static const struct db_fields ampereone_payload5_event_fields[] = {
	{ .name = "id",		.type = "INTEGER PRIMARY KEY" },
	{ .name = "timestamp",	.type = "TEXT" },
	{ .name = "type_id",	.type = "TEXT" },
	{ .name = "subtype_id",	.type = "TEXT" },
	{ .name = "instance",	.type = "INTEGER" },
	{ .name = "socket_num",	.type = "INTEGER" },
};

static const struct db_table_descriptor ampereone_payload5_event_tab = {
	.name = "ampereone_payload5_event",
	.fields = ampereone_payload5_event_fields,
	.num_fields = ARRAY_SIZE(ampereone_payload5_event_fields),
};

/*key pair definition for ampere specific error payload type 6*/
static const struct db_fields ampereone_payload6_event_fields[] = {
	{ .name = "id",		.type = "INTEGER PRIMARY KEY" },
	{ .name = "timestamp",	.type = "TEXT" },
	{ .name = "type_id",	.type = "TEXT" },
	{ .name = "subtype_id",	.type = "TEXT" },
	{ .name = "instance",	.type = "INTEGER" },
	{ .name = "socket_num",	.type = "INTEGER" },
	{ .name = "DRIVER",	.type = "INTEGER" },
	{ .name = "ERROR_CODE",	.type = "INTEGER" },
	{ .name = "MSG_SIZE",	.type = "INTEGER" },
	{ .name = "ERROR_MSG",	.type = "TEXT" },
};

static const struct db_table_descriptor ampereone_payload6_event_tab = {
	.name = "ampereone_payload6_event",
	.fields = ampereone_payload6_event_fields,
	.num_fields = ARRAY_SIZE(ampereone_payload6_event_fields),
};



/*Save data with different type into sqlite3 db*/
static void record_ampereone_data(struct ras_ns_ev_decoder *ev_decoder,
			    enum ampereone_oem_data_type data_type,
			    int id, int64_t data, const char *text)
{
	switch (data_type) {
	case AMPEREONE_OEM_DATA_TYPE_INT:
		/*
		 * Use this type sparingly. Just use INT64 for most things.
		 * The reason is that most (if not all) of our registers contain
		 * unsigned 32 bit values. SQLite doesn't have unsigned data types.
		 * So, Let's use SQLite's INT64 so that the full range of our unsigned
		 * 32 bits are accurately represented in the SQLite DB.
		 */
		sqlite3_bind_int(ev_decoder->stmt_dec_record, id, data);
		break;
	case AMPEREONE_OEM_DATA_TYPE_INT64:
		sqlite3_bind_int64(ev_decoder->stmt_dec_record, id, data);
		break;
	case AMPEREONE_OEM_DATA_TYPE_TEXT:
		sqlite3_bind_text(ev_decoder->stmt_dec_record, id,
				  text, -1, NULL);
		break;
	default:
		break;
	}
}

static int store_ampereone_err_data(struct ras_ns_ev_decoder *ev_decoder,
			      const char *name)
{
	int rc;

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

/*save all Ampere Specific Error Payload type 0 to sqlite3 database*/
static void record_ampereone_payload0_err(struct ras_ns_ev_decoder *ev_decoder,
				    const char *type_str, const char *subtype_str,
				    const struct ampereone_payload0_type_sec *err)
{
	if (ev_decoder) {
		record_ampereone_data(ev_decoder, AMPEREONE_OEM_DATA_TYPE_TEXT,
				AMPEREONE_PAYLOAD0_FIELD_TYPE_ID, 0, type_str);
		record_ampereone_data(ev_decoder, AMPEREONE_OEM_DATA_TYPE_TEXT,
				AMPEREONE_PAYLOAD0_FIELD_SUB_TYPE_ID, 0, subtype_str);
		record_ampereone_data(ev_decoder, AMPEREONE_OEM_DATA_TYPE_INT,
				AMPEREONE_PAYLOAD0_FIELD_INS,
				AMPEREONE_INSTANCE(err->header.instance), NULL);
		record_ampereone_data(ev_decoder, AMPEREONE_OEM_DATA_TYPE_INT,
				AMPEREONE_PAYLOAD0_FIELD_SOCKET_NUM,
				AMPEREONE_SOCKET_NUM(err->header.instance), NULL);

		record_ampereone_data(ev_decoder, AMPEREONE_OEM_DATA_TYPE_INT64,
				AMPEREONE_PAYLOAD0_FIELD_ERRxFR, err->err_fr, NULL);
		record_ampereone_data(ev_decoder, AMPEREONE_OEM_DATA_TYPE_INT64,
				AMPEREONE_PAYLOAD0_FIELD_ERRxCTLR,
				err->err_ctlr, NULL);
		record_ampereone_data(ev_decoder, AMPEREONE_OEM_DATA_TYPE_INT64,
				AMPEREONE_PAYLOAD0_FIELD_ERRxSTATUS,
				err->err_status, NULL);
		record_ampereone_data(ev_decoder, AMPEREONE_OEM_DATA_TYPE_INT64,
				AMPEREONE_PAYLOAD0_FIELD_ERRxADDR,
				err->err_addr, NULL);
		record_ampereone_data(ev_decoder, AMPEREONE_OEM_DATA_TYPE_INT64,
				AMPEREONE_PAYLOAD0_FIELD_ERRxMISC0,
				err->err_misc_0, NULL);
		record_ampereone_data(ev_decoder, AMPEREONE_OEM_DATA_TYPE_INT64,
				AMPEREONE_PAYLOAD0_FIELD_ERRxMISC1,
				err->err_misc_1, NULL);
		record_ampereone_data(ev_decoder, AMPEREONE_OEM_DATA_TYPE_INT64,
				AMPEREONE_PAYLOAD0_FIELD_ERRxMISC2,
				err->err_misc_2, NULL);
		record_ampereone_data(ev_decoder, AMPEREONE_OEM_DATA_TYPE_INT64,
				AMPEREONE_PAYLOAD0_FIELD_ERRxMISC3,
				err->err_misc_3, NULL);
		store_ampereone_err_data(ev_decoder, "ampereone_payload0_event_tab");
	}
}

/*save all Ampere Specific Error Payload type 1 to sqlite3 database*/
static void record_ampereone_payload1_err(struct ras_ns_ev_decoder *ev_decoder,
				    const char *type_str, const char *subtype_str,
				    const struct ampereone_payload1_type_sec *err)
{
	if (ev_decoder) {
		record_ampereone_data(ev_decoder, AMPEREONE_OEM_DATA_TYPE_TEXT,
				AMPEREONE_PAYLOAD1_FIELD_TYPE_ID, 0, type_str);
		record_ampereone_data(ev_decoder, AMPEREONE_OEM_DATA_TYPE_TEXT,
				AMPEREONE_PAYLOAD1_FIELD_SUB_TYPE_ID, 0, subtype_str);
		record_ampereone_data(ev_decoder, AMPEREONE_OEM_DATA_TYPE_INT,
				AMPEREONE_PAYLOAD1_FIELD_INS,
				AMPEREONE_INSTANCE(err->header.instance), NULL);
		record_ampereone_data(ev_decoder, AMPEREONE_OEM_DATA_TYPE_INT,
				AMPEREONE_PAYLOAD1_FIELD_SOCKET_NUM,
				AMPEREONE_SOCKET_NUM(err->header.instance), NULL);

		record_ampereone_data(ev_decoder, AMPEREONE_OEM_DATA_TYPE_INT64,
				AMPEREONE_PAYLOAD1_FIELD_AER_CORR_ERR_STATUS,
				err->aer_ue_err_status, NULL);
		record_ampereone_data(ev_decoder, AMPEREONE_OEM_DATA_TYPE_INT64,
				AMPEREONE_PAYLOAD1_FIELD_AER_UNCORR_ERR_STATUS,
				err->aer_ce_err_status, NULL);
		record_ampereone_data(ev_decoder, AMPEREONE_OEM_DATA_TYPE_INT64,
				AMPEREONE_PAYLOAD1_FIELD_EBUF_OVERFLOW,
				err->ebuf_overflow, NULL);
		record_ampereone_data(ev_decoder, AMPEREONE_OEM_DATA_TYPE_INT64,
				AMPEREONE_PAYLOAD1_FIELD_EBUF_UNDERRUN,
				err->ebuf_underrun, NULL);
		record_ampereone_data(ev_decoder, AMPEREONE_OEM_DATA_TYPE_INT64,
				AMPEREONE_PAYLOAD1_FIELD_DECODE_ERROR,
				err->decode_error, NULL);
		record_ampereone_data(ev_decoder, AMPEREONE_OEM_DATA_TYPE_INT64,
				AMPEREONE_PAYLOAD1_FIELD_RUNNING_DISPARITY_ERROR,
				err->running_disparity_error, NULL);
		record_ampereone_data(ev_decoder, AMPEREONE_OEM_DATA_TYPE_INT64,
				AMPEREONE_PAYLOAD1_FIELD_SKP_OS_PARITY_ERROR_GEN3,
				err->skp_os_parity_error_gen3, NULL);
		record_ampereone_data(ev_decoder, AMPEREONE_OEM_DATA_TYPE_INT64,
				AMPEREONE_PAYLOAD1_FIELD_SYNC_HEADER_ERROR,
				err->sync_header_error, NULL);
		record_ampereone_data(ev_decoder, AMPEREONE_OEM_DATA_TYPE_INT64,
				AMPEREONE_PAYLOAD1_FIELD_RX_VALID_DEASSERTION,
				err->rx_valid_deassertion, NULL);
		record_ampereone_data(ev_decoder, AMPEREONE_OEM_DATA_TYPE_INT64,
				AMPEREONE_PAYLOAD1_FIELD_CTL_SKP_OS_PARITY_ERROR_GEN4,
				err->ctl_skp_os_parity_error_gen4, NULL);
		record_ampereone_data(ev_decoder, AMPEREONE_OEM_DATA_TYPE_INT64,
				AMPEREONE_PAYLOAD1_FIELD_FIRST_RETIMER_PARITY_ERROR_GEN4,
				err->first_retimer_parity_error_gen4, NULL);
		record_ampereone_data(ev_decoder, AMPEREONE_OEM_DATA_TYPE_INT64,
				AMPEREONE_PAYLOAD1_FIELD_SECOND_RETIMER_PARITY_ERROR_GEN4,
				err->second_retimer_parity_error_gen4, NULL);
		record_ampereone_data(ev_decoder, AMPEREONE_OEM_DATA_TYPE_INT64,
				AMPEREONE_PAYLOAD1_FIELD_MARGIN_CRC_AND_PARTIY_ERROR_GEN4,
				err->margin_crc_and_parity_error_gen4, NULL);
		record_ampereone_data(ev_decoder, AMPEREONE_OEM_DATA_TYPE_INT64,
				AMPEREONE_PAYLOAD1_FIELD_RASDES_GROUP1_COUNTERS,
				err->rasdes_group1_counters, NULL);
		record_ampereone_data(ev_decoder, AMPEREONE_OEM_DATA_TYPE_INT64,
				AMPEREONE_PAYLOAD1_FIELD_RSVD0,
				err->rsvd0, NULL);
		record_ampereone_data(ev_decoder, AMPEREONE_OEM_DATA_TYPE_INT64,
				AMPEREONE_PAYLOAD1_FIELD_RASDES_GROUP2_COUNTERS,
				err->rasdes_group2_counters, NULL);
		record_ampereone_data(ev_decoder, AMPEREONE_OEM_DATA_TYPE_INT64,
				AMPEREONE_PAYLOAD1_FIELD_EBUF_SKP_ADD,
				err->ebuf_skp_add, NULL);
		record_ampereone_data(ev_decoder, AMPEREONE_OEM_DATA_TYPE_INT64,
				AMPEREONE_PAYLOAD1_FIELD_EBUF_SKP_DEL,
				err->ebuf_skp_del, NULL);
		record_ampereone_data(ev_decoder, AMPEREONE_OEM_DATA_TYPE_INT64,
				AMPEREONE_PAYLOAD1_FIELD_RASDES_GROUP5_COUNTERS,
				err->rasdes_group5_counters, NULL);
		record_ampereone_data(ev_decoder, AMPEREONE_OEM_DATA_TYPE_INT64,
				AMPEREONE_PAYLOAD1_FIELD_RASDES_GROUP5_COUNTERS_CONTINUED,
				err->rasdes_group5_counters_continued, NULL);
		record_ampereone_data(ev_decoder, AMPEREONE_OEM_DATA_TYPE_INT64,
				AMPEREONE_PAYLOAD1_FIELD_RSVD1,
				err->rsvd1, NULL);
		record_ampereone_data(ev_decoder, AMPEREONE_OEM_DATA_TYPE_INT64,
				AMPEREONE_PAYLOAD1_FIELD_SI_DEBUG_LAYER1_STATUS_LANE0,
				err->dbg_l1_status_lane0, NULL);
		record_ampereone_data(ev_decoder, AMPEREONE_OEM_DATA_TYPE_INT64,
				AMPEREONE_PAYLOAD1_FIELD_SI_DEBUG_LAYER1_STATUS_LANE1,
				err->dbg_l1_status_lane0, NULL);
		record_ampereone_data(ev_decoder, AMPEREONE_OEM_DATA_TYPE_INT64,
				AMPEREONE_PAYLOAD1_FIELD_SI_DEBUG_LAYER1_STATUS_LANE2,
				err->dbg_l1_status_lane1, NULL);
		record_ampereone_data(ev_decoder, AMPEREONE_OEM_DATA_TYPE_INT64,
				AMPEREONE_PAYLOAD1_FIELD_SI_DEBUG_LAYER1_STATUS_LANE3,
				err->dbg_l1_status_lane2, NULL);
		record_ampereone_data(ev_decoder, AMPEREONE_OEM_DATA_TYPE_INT64,
				AMPEREONE_PAYLOAD1_FIELD_SI_DEBUG_LAYER1_STATUS_LANE4,
				err->dbg_l1_status_lane3, NULL);
		record_ampereone_data(ev_decoder, AMPEREONE_OEM_DATA_TYPE_INT64,
				AMPEREONE_PAYLOAD1_FIELD_SI_DEBUG_LAYER1_STATUS_LANE5,
				err->dbg_l1_status_lane4, NULL);
		record_ampereone_data(ev_decoder, AMPEREONE_OEM_DATA_TYPE_INT64,
				AMPEREONE_PAYLOAD1_FIELD_SI_DEBUG_LAYER1_STATUS_LANE6,
				err->dbg_l1_status_lane5, NULL);
		record_ampereone_data(ev_decoder, AMPEREONE_OEM_DATA_TYPE_INT64,
				AMPEREONE_PAYLOAD1_FIELD_SI_DEBUG_LAYER1_STATUS_LANE7,
				err->dbg_l1_status_lane6, NULL);
		record_ampereone_data(ev_decoder, AMPEREONE_OEM_DATA_TYPE_INT64,
				AMPEREONE_PAYLOAD1_FIELD_SI_DEBUG_LAYER1_STATUS_LANE8,
				err->dbg_l1_status_lane7, NULL);
		record_ampereone_data(ev_decoder, AMPEREONE_OEM_DATA_TYPE_INT64,
				AMPEREONE_PAYLOAD1_FIELD_SI_DEBUG_LAYER1_STATUS_LANE9,
				err->dbg_l1_status_lane8, NULL);
		record_ampereone_data(ev_decoder, AMPEREONE_OEM_DATA_TYPE_INT64,
				AMPEREONE_PAYLOAD1_FIELD_SI_DEBUG_LAYER1_STATUS_LANE10,
				err->dbg_l1_status_lane9, NULL);
		record_ampereone_data(ev_decoder, AMPEREONE_OEM_DATA_TYPE_INT64,
				AMPEREONE_PAYLOAD1_FIELD_SI_DEBUG_LAYER1_STATUS_LANE11,
				err->dbg_l1_status_lane10, NULL);
		record_ampereone_data(ev_decoder, AMPEREONE_OEM_DATA_TYPE_INT64,
				AMPEREONE_PAYLOAD1_FIELD_SI_DEBUG_LAYER1_STATUS_LANE12,
				err->dbg_l1_status_lane11, NULL);
		record_ampereone_data(ev_decoder, AMPEREONE_OEM_DATA_TYPE_INT64,
				AMPEREONE_PAYLOAD1_FIELD_SI_DEBUG_LAYER1_STATUS_LANE13,
				err->dbg_l1_status_lane12, NULL);
		record_ampereone_data(ev_decoder, AMPEREONE_OEM_DATA_TYPE_INT64,
				AMPEREONE_PAYLOAD1_FIELD_SI_DEBUG_LAYER1_STATUS_LANE14,
				err->dbg_l1_status_lane13, NULL);
		record_ampereone_data(ev_decoder, AMPEREONE_OEM_DATA_TYPE_INT64,
				AMPEREONE_PAYLOAD1_FIELD_SI_DEBUG_LAYER1_STATUS_LANE15,
				err->dbg_l1_status_lane14, NULL);

		store_ampereone_err_data(ev_decoder, "ampereone_payload1_event_tab");
	}
}

/*save all Ampere Specific Error Payload type 2 to sqlite3 database*/
static void record_ampereone_payload2_err(struct ras_ns_ev_decoder *ev_decoder,
				    const char *type_str, const char *subtype_str,
				    const struct ampereone_payload2_type_sec *err)
{
	if (ev_decoder) {
		record_ampereone_data(ev_decoder, AMPEREONE_OEM_DATA_TYPE_TEXT,
				AMPEREONE_PAYLOAD2_FIELD_TYPE_ID, 0, type_str);
		record_ampereone_data(ev_decoder, AMPEREONE_OEM_DATA_TYPE_TEXT,
				AMPEREONE_PAYLOAD2_FIELD_SUB_TYPE_ID, 0, subtype_str);
		record_ampereone_data(ev_decoder, AMPEREONE_OEM_DATA_TYPE_INT,
				AMPEREONE_PAYLOAD2_FIELD_INS,
				AMPEREONE_INSTANCE(err->header.instance), NULL);
		record_ampereone_data(ev_decoder, AMPEREONE_OEM_DATA_TYPE_INT,
				AMPEREONE_PAYLOAD2_FIELD_SOCKET_NUM,
				AMPEREONE_SOCKET_NUM(err->header.instance), NULL);

		record_ampereone_data(ev_decoder, AMPEREONE_OEM_DATA_TYPE_INT64,
				AMPEREONE_PAYLOAD2_FIELD_CORR_COUNT_REPORT,
				err->corr_count_report, NULL);
		record_ampereone_data(ev_decoder, AMPEREONE_OEM_DATA_TYPE_INT64,
				AMPEREONE_PAYLOAD2_FIELD_CORR_ERROR_LOCATION,
				err->corr_error_location, NULL);
		record_ampereone_data(ev_decoder, AMPEREONE_OEM_DATA_TYPE_INT64,
				AMPEREONE_PAYLOAD2_FIELD_RAM_ADDR_CORR,
				err->ram_addr_corr, NULL);
		record_ampereone_data(ev_decoder, AMPEREONE_OEM_DATA_TYPE_INT64,
				AMPEREONE_PAYLOAD2_FIELD_UNCORR_COUNT_REPORT,
				err->uncorr_count_report, NULL);
		record_ampereone_data(ev_decoder, AMPEREONE_OEM_DATA_TYPE_INT64,
				AMPEREONE_PAYLOAD2_FIELD_UNCORR_ERROR_LOCATION,
				err->uncorr_error_location, NULL);
		record_ampereone_data(ev_decoder, AMPEREONE_OEM_DATA_TYPE_INT64,
				AMPEREONE_PAYLOAD2_FIELD_RAM_ADDR_UNCORR,
				err->ram_addr_uncorr, NULL);
		store_ampereone_err_data(ev_decoder, "ampereone_payload2_event_tab");
	}
}

/*save all Ampere Specific Error Payload type 3 to sqlite3 database*/
static void record_ampereone_payload3_err(struct ras_ns_ev_decoder *ev_decoder,
				    const char *type_str, const char *subtype_str,
				    const struct ampereone_payload3_type_sec *err)
{
	if (ev_decoder) {
		record_ampereone_data(ev_decoder, AMPEREONE_OEM_DATA_TYPE_TEXT,
				AMPEREONE_PAYLOAD3_FIELD_TYPE_ID, 0, type_str);
		record_ampereone_data(ev_decoder, AMPEREONE_OEM_DATA_TYPE_TEXT,
				AMPEREONE_PAYLOAD3_FIELD_SUB_TYPE_ID, 0, subtype_str);
		record_ampereone_data(ev_decoder, AMPEREONE_OEM_DATA_TYPE_INT,
				AMPEREONE_PAYLOAD3_FIELD_INS,
				AMPEREONE_INSTANCE(err->header.instance), NULL);
		record_ampereone_data(ev_decoder, AMPEREONE_OEM_DATA_TYPE_INT,
				AMPEREONE_PAYLOAD3_FIELD_SOCKET_NUM,
				AMPEREONE_SOCKET_NUM(err->header.instance), NULL);

		record_ampereone_data(ev_decoder, AMPEREONE_OEM_DATA_TYPE_INT64,
				AMPEREONE_PAYLOAD3_FIELD_ECC_ADDRESS,
				err->ecc_addr, NULL);
		record_ampereone_data(ev_decoder, AMPEREONE_OEM_DATA_TYPE_INT64,
				AMPEREONE_PAYLOAD3_FIELD_ECC_DATA,
				err->ecc_data, NULL);
		record_ampereone_data(ev_decoder, AMPEREONE_OEM_DATA_TYPE_INT64,
				AMPEREONE_PAYLOAD3_FIELD_ECC_SRC_ID,
				err->ecc_id, NULL);
		record_ampereone_data(ev_decoder, AMPEREONE_OEM_DATA_TYPE_INT64,
				AMPEREONE_PAYLOAD3_FIELD_ECC_SYND,
				err->ecc_synd, NULL);
		record_ampereone_data(ev_decoder, AMPEREONE_OEM_DATA_TYPE_INT64,
				AMPEREONE_PAYLOAD3_FIELD_ECC_MCE_CNT,
				err->ecc_mce_cnt, NULL);
		record_ampereone_data(ev_decoder, AMPEREONE_OEM_DATA_TYPE_INT64,
				AMPEREONE_PAYLOAD3_FIELD_ECC_CTLR,
				err->ecc_ctlr, NULL);
		record_ampereone_data(ev_decoder, AMPEREONE_OEM_DATA_TYPE_INT64,
				AMPEREONE_PAYLOAD3_FIELD_ECC_ERR_STS,
				err->ecc_err_sts, NULL);
		record_ampereone_data(ev_decoder, AMPEREONE_OEM_DATA_TYPE_INT64,
				AMPEREONE_PAYLOAD3_FIELD_ECC_ERR_CNT,
				err->ecc_err_cnt, NULL);
		store_ampereone_err_data(ev_decoder, "ampereone_payload3_event_tab");
	}
}

/*save all Ampere Specific Error Payload type 4 to sqlite3 database*/
static void record_ampereone_payload4_err(struct ras_ns_ev_decoder *ev_decoder,
				    const char *type_str, const char *subtype_str,
				    const struct ampereone_payload4_type_sec *err)
{
	if (ev_decoder) {
		record_ampereone_data(ev_decoder, AMPEREONE_OEM_DATA_TYPE_TEXT,
				AMPEREONE_PAYLOAD4_FIELD_TYPE_ID, 0, type_str);
		record_ampereone_data(ev_decoder, AMPEREONE_OEM_DATA_TYPE_TEXT,
				AMPEREONE_PAYLOAD4_FIELD_SUB_TYPE_ID, 0, subtype_str);
		record_ampereone_data(ev_decoder, AMPEREONE_OEM_DATA_TYPE_INT,
				AMPEREONE_PAYLOAD4_FIELD_INS,
				AMPEREONE_INSTANCE(err->header.instance), NULL);
		record_ampereone_data(ev_decoder, AMPEREONE_OEM_DATA_TYPE_INT,
				AMPEREONE_PAYLOAD4_FIELD_SOCKET_NUM,
				AMPEREONE_SOCKET_NUM(err->header.instance), NULL);

		record_ampereone_data(ev_decoder, AMPEREONE_OEM_DATA_TYPE_INT64,
				AMPEREONE_PAYLOAD4_FIELD_ADDRESS,
				err->address, NULL);
		record_ampereone_data(ev_decoder, AMPEREONE_OEM_DATA_TYPE_INT64,
				AMPEREONE_PAYLOAD4_FIELD_SRC_ID,
				err->srcid, NULL);
		record_ampereone_data(ev_decoder, AMPEREONE_OEM_DATA_TYPE_INT64,
				AMPEREONE_PAYLOAD4_FIELD_TXNID,
				err->txnid, NULL);
		record_ampereone_data(ev_decoder, AMPEREONE_OEM_DATA_TYPE_INT64,
				AMPEREONE_PAYLOAD4_FIELD_TYPE,
				err->type, NULL);
		record_ampereone_data(ev_decoder, AMPEREONE_OEM_DATA_TYPE_INT64,
				AMPEREONE_PAYLOAD4_FIELD_LPID,
				err->lpid, NULL);
		record_ampereone_data(ev_decoder, AMPEREONE_OEM_DATA_TYPE_INT64,
				AMPEREONE_PAYLOAD4_FIELD_OPCODE,
				err->opcode, NULL);
		record_ampereone_data(ev_decoder, AMPEREONE_OEM_DATA_TYPE_INT64,
				AMPEREONE_PAYLOAD4_FIELD_TAG,
				err->tag, NULL);
		record_ampereone_data(ev_decoder, AMPEREONE_OEM_DATA_TYPE_INT64,
				AMPEREONE_PAYLOAD4_FIELD_MPAM,
				err->mpam, NULL);

		store_ampereone_err_data(ev_decoder, "ampereone_payload4_event_tab");
	}
}

/*save all Ampere Specific Error Payload type 5 to sqlite3 database*/
static void record_ampereone_payload5_err(struct ras_ns_ev_decoder *ev_decoder,
				    const char *type_str, const char *subtype_str,
				    const struct ampereone_payload5_type_sec *err)
{
	if (ev_decoder) {
		record_ampereone_data(ev_decoder, AMPEREONE_OEM_DATA_TYPE_TEXT,
				AMPEREONE_PAYLOAD5_FIELD_TYPE_ID, 0, type_str);
		record_ampereone_data(ev_decoder, AMPEREONE_OEM_DATA_TYPE_TEXT,
				AMPEREONE_PAYLOAD5_FIELD_SUB_TYPE_ID, 0, subtype_str);
		record_ampereone_data(ev_decoder, AMPEREONE_OEM_DATA_TYPE_INT,
				AMPEREONE_PAYLOAD5_FIELD_INS,
				AMPEREONE_INSTANCE(err->header.instance), NULL);
		record_ampereone_data(ev_decoder, AMPEREONE_OEM_DATA_TYPE_INT,
				AMPEREONE_PAYLOAD5_FIELD_SOCKET_NUM,
				AMPEREONE_SOCKET_NUM(err->header.instance), NULL);

		store_ampereone_err_data(ev_decoder, "ampereone_payload5_event_tab");
	}
}

/*save all Ampere Specific Error Payload type 6 to sqlite3 database*/
static void record_ampereone_payload6_err(struct ras_ns_ev_decoder *ev_decoder,
				    const char *type_str, const char *subtype_str,
				    const struct ampereone_payload6_type_sec *err)
{
	int64_t nothing = 0;

	if (ev_decoder) {
		record_ampereone_data(ev_decoder, AMPEREONE_OEM_DATA_TYPE_TEXT,
				AMPEREONE_PAYLOAD6_FIELD_TYPE_ID, 0, type_str);
		record_ampereone_data(ev_decoder, AMPEREONE_OEM_DATA_TYPE_TEXT,
				AMPEREONE_PAYLOAD6_FIELD_SUB_TYPE_ID, 0, subtype_str);
		record_ampereone_data(ev_decoder, AMPEREONE_OEM_DATA_TYPE_INT,
				AMPEREONE_PAYLOAD6_FIELD_INS,
				AMPEREONE_INSTANCE(err->header.instance), NULL);
		record_ampereone_data(ev_decoder, AMPEREONE_OEM_DATA_TYPE_INT,
				AMPEREONE_PAYLOAD6_FIELD_SOCKET_NUM,
				AMPEREONE_SOCKET_NUM(err->header.instance), NULL);

		record_ampereone_data(ev_decoder, AMPEREONE_OEM_DATA_TYPE_INT64,
				AMPEREONE_PAYLOAD6_FIELD_DRIVER,
				err->driver, NULL);
		record_ampereone_data(ev_decoder, AMPEREONE_OEM_DATA_TYPE_INT64,
				AMPEREONE_PAYLOAD6_FIELD_ERROR_CODE,
				err->error_code, NULL);
		record_ampereone_data(ev_decoder, AMPEREONE_OEM_DATA_TYPE_INT64,
				AMPEREONE_PAYLOAD6_FIELD_MSG_SIZE,
				err->error_msg_size, NULL);
		record_ampereone_data(ev_decoder, AMPEREONE_OEM_DATA_TYPE_TEXT,
				AMPEREONE_PAYLOAD6_FIELD_ERROR_MSG,
				nothing, (const char *)err->error_msg);

		store_ampereone_err_data(ev_decoder, "ampereone_payload6_event_tab");
	}
}

#else
static void record_ampereone_data(struct ras_ns_ev_decoder *ev_decoder,
			    enum ampereone_oem_data_type data_type,
			    int id, int64_t data, const char *text)
{
}

static void record_ampereone_payload0_err(struct ras_ns_ev_decoder *ev_decoder,
				    const char *type_str, const char *subtype_str,
				    const struct ampereone_payload0_type_sec *err)
{
}

static void record_ampereone_payload1_err(struct ras_ns_ev_decoder *ev_decoder,
				    const char *type_str, const char *subtype_str,
				    const struct ampereone_payload1_type_sec *err)
{
}

static void record_ampereone_payload2_err(struct ras_ns_ev_decoder *ev_decoder,
				    const char *type_str, const char *subtype_str,
				    const struct ampereone_payload2_type_sec *err)
{
}

static void record_ampereone_payload3_err(struct ras_ns_ev_decoder *ev_decoder,
				    const char *type_str, const char *subtype_str,
				    const struct ampereone_payload3_type_sec *err)
{
}

static void record_ampereone_payload4_err(struct ras_ns_ev_decoder *ev_decoder,
				    const char *type_str, const char *subtype_str,
				    const struct ampereone_payload4_type_sec *err)
{
}

static void record_ampereone_payload5_err(struct ras_ns_ev_decoder *ev_decoder,
				    const char *type_str, const char *subtype_str,
				    const struct ampereone_payload5_type_sec *err)
{
}

static void record_ampereone_payload6_err(struct ras_ns_ev_decoder *ev_decoder,
				    const char *type_str, const char *subtype_str,
				    const struct ampereone_payload6_type_sec *err)
{
}

static int store_ampereone_err_data(struct ras_ns_ev_decoder *ev_decoder, char *name)
{
	return 0;
}
#endif

void decode_ampereone_payload0_err_regs(struct ras_ns_ev_decoder *ev_decoder,
				struct trace_seq *s,
				const struct ampereone_payload0_type_sec *err)
{
	char buf[AMPEREONE_PAYLOAD0_BUF_LEN];
	char *p = buf;
	char *end = buf + AMPEREONE_PAYLOAD0_BUF_LEN;
	int i = 0;
	const char *subtype_str;

	const char *type_str = ampereone_type_name(ampereone_payload_error_type,
					    AMPEREONE_TYPE(err->header.type));


	subtype_str  = ampereone_subtype_name(ampereone_payload_error_type,
					    AMPEREONE_TYPE(err->header.type), err->header.subtype);

	trace_seq_printf(s, " Payload Type: 0\n");

	//display error type
	p += snprintf(p, end - p, " %s", disp_ampereone_payload0_err_reg_name[i++]);
	p += snprintf(p, end - p, " %s\n", type_str);

	//display error subtype
	p += snprintf(p, end - p, " %s", disp_ampereone_payload0_err_reg_name[i++]);
	p += snprintf(p, end - p, " %s\n", subtype_str);

	//display error instance
	p += snprintf(p, end - p, " %s", disp_ampereone_payload0_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%x\n", AMPEREONE_INSTANCE(err->header.instance));

	//display socket number
	p += snprintf(p, end - p, " %s",
	disp_ampereone_payload0_err_reg_name[i++]);
	p += snprintf(p, end - p, " %d\n", AMPEREONE_SOCKET_NUM(err->header.instance));

	// Display ERRxFR
	p += snprintf(p, end - p, " %s", disp_ampereone_payload0_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%llx\n",
		     (unsigned long long)err->err_fr);

	// Display ERRxCTRL
	p += snprintf(p, end - p, " %s", disp_ampereone_payload0_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%llx\n",
		     (unsigned long long)err->err_ctlr);

	//display status register
	p += snprintf(p, end - p, " %s", disp_ampereone_payload0_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%llx\n",
		     (unsigned long long)err->err_status);

	//display address register
	p += snprintf(p, end - p, " %s", disp_ampereone_payload0_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%llx\n",
		     (unsigned long long)err->err_addr);

	//display MISC0
	p += snprintf(p, end - p, " %s", disp_ampereone_payload0_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%llx\n",
		     (unsigned long long)err->err_misc_0);

	//display MISC1
	p += snprintf(p, end - p, " %s", disp_ampereone_payload0_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%llx\n",
		     (unsigned long long)err->err_misc_1);

	//display MISC2
	p += snprintf(p, end - p, " %s", disp_ampereone_payload0_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%llx\n",
		     (unsigned long long)err->err_misc_2);

	//display MISC3
	p += snprintf(p, end - p, " %s", disp_ampereone_payload0_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%llx\n",
		     (unsigned long long)err->err_misc_3);

	if (p > buf && p < end) {
		p--;
		*p = '\0';
	}

	record_ampereone_payload0_err(ev_decoder, type_str, subtype_str, err);
	i = 0;
	p = NULL;
	end = NULL;
	trace_seq_printf(s, "%s\n", buf);
}

// Payload Type 1: PCIe AER Format
void decode_ampereone_payload1_err_regs(struct ras_ns_ev_decoder *ev_decoder,
				struct trace_seq *s,
				const struct ampereone_payload1_type_sec *err)
{
	char buf[AMPEREONE_PAYLOAD1_BUF_LEN];
	char *p = buf;
	char *end = buf + AMPEREONE_PAYLOAD1_BUF_LEN;
	int i = 0;
	const char *subtype_str;

	const char *type_str = ampereone_type_name(ampereone_payload_error_type,
					    AMPEREONE_TYPE(err->header.type));


	subtype_str  = ampereone_subtype_name(ampereone_payload_error_type,
					    AMPEREONE_TYPE(err->header.type), err->header.subtype);

	trace_seq_printf(s, " Payload Type: 1\n");

	// Display error type
	p += snprintf(p, end - p, " %s", disp_ampereone_payload1_err_reg_name[i++]);
	p += snprintf(p, end - p, " %s\n", type_str);

	// Display error subtype
	p += snprintf(p, end - p, " %s", disp_ampereone_payload1_err_reg_name[i++]);
	p += snprintf(p, end - p, " %s\n", subtype_str);

	// Display error instance
	p += snprintf(p, end - p, " %s", disp_ampereone_payload1_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%x\n", AMPEREONE_INSTANCE(err->header.instance));

	// Display socket number
	p += snprintf(p, end - p, " %s", disp_ampereone_payload1_err_reg_name[i++]);
	p += snprintf(p, end - p, " %d\n", AMPEREONE_SOCKET_NUM(err->header.instance));

	//display UE Error Status
	p += snprintf(p, end - p, " %s", disp_ampereone_payload1_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%lx\n",
		     (unsigned long)err->aer_ue_err_status);

	//display CE Error Status
	p += snprintf(p, end - p, " %s", disp_ampereone_payload1_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%lx\n",
		     (unsigned long)err->aer_ce_err_status);

	//display Group 0, Event 0
	p += snprintf(p, end - p, " %s", disp_ampereone_payload1_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%llx\n",
		     (unsigned long long)err->ebuf_overflow);

	//display Group 0, Event 1
	p += snprintf(p, end - p, " %s", disp_ampereone_payload1_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%llx\n",
		     (unsigned long long)err->ebuf_underrun);

	//display Group 0, Event 2
	p += snprintf(p, end - p, " %s", disp_ampereone_payload1_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%llx\n",
		     (unsigned long long)err->decode_error);

	//display Group 0, Event 3
	p += snprintf(p, end - p, " %s", disp_ampereone_payload1_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%llx\n",
		     (unsigned long long)err->running_disparity_error);

	//display Group 0, Event 4
	p += snprintf(p, end - p, " %s", disp_ampereone_payload1_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%llx\n",
		     (unsigned long long)err->skp_os_parity_error_gen3);

	//display Group 0, Event 5
	p += snprintf(p, end - p, " %s", disp_ampereone_payload1_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%llx\n",
		     (unsigned long long)err->sync_header_error);

	//display Group 0, Event 6
	p += snprintf(p, end - p, " %s", disp_ampereone_payload1_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%llx\n",
		     (unsigned long long)err->rx_valid_deassertion);

	//display Group 0, Event 7
	p += snprintf(p, end - p, " %s", disp_ampereone_payload1_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%llx\n",
		     (unsigned long long)err->ctl_skp_os_parity_error_gen4);

	//display Group 0, Event 8
	p += snprintf(p, end - p, " %s", disp_ampereone_payload1_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%llx\n",
		     (unsigned long long)err->first_retimer_parity_error_gen4);

	//display Group 0, Event 9
	p += snprintf(p, end - p, " %s", disp_ampereone_payload1_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%llx\n",
		     (unsigned long long)err->second_retimer_parity_error_gen4);

	//display Group 0, Event 10
	p += snprintf(p, end - p, " %s", disp_ampereone_payload1_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%llx\n",
		     (unsigned long long)err->margin_crc_and_parity_error_gen4);

	//display Group 1 counters
	p += snprintf(p, end - p, " %s", disp_ampereone_payload1_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%llx\n",
		     (unsigned long long)err->rasdes_group1_counters);

	//display rsvd0
	p += snprintf(p, end - p, " %s", disp_ampereone_payload1_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%llx\n",
		     (unsigned long long)err->rsvd0);

	//display Group 2 counters
	p += snprintf(p, end - p, " %s", disp_ampereone_payload1_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%llx\n",
		     (unsigned long long)err->rasdes_group2_counters);

	//display Group 4, Event 0
	p += snprintf(p, end - p, " %s", disp_ampereone_payload1_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%llx\n",
		     (unsigned long long)err->ebuf_skp_add);

	//display Group 4, Event 1
	p += snprintf(p, end - p, " %s", disp_ampereone_payload1_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%llx\n",
		     (unsigned long long)err->ebuf_skp_del);

	//display Group 5 counters ...
	p += snprintf(p, end - p, " %s", disp_ampereone_payload1_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%llx\n",
		     (unsigned long long)err->rasdes_group5_counters);

	//display Group 5 counters continued.
	p += snprintf(p, end - p, " %s", disp_ampereone_payload1_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%llx\n",
		     (unsigned long long)err->rasdes_group5_counters_continued);

	//display rsvd1
	p += snprintf(p, end - p, " %s", disp_ampereone_payload1_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%llx\n",
		     (unsigned long long)err->rsvd1);

	//display Silicon Debug Layer1 Status Lane 0 ...
	p += snprintf(p, end - p, " %s", disp_ampereone_payload1_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%lx\n",
		     (unsigned long)err->dbg_l1_status_lane0);

	//display Silicon Debug Layer1 Status Lane 1 ...
	p += snprintf(p, end - p, " %s", disp_ampereone_payload1_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%lx\n",
		     (unsigned long)err->dbg_l1_status_lane1);

	//display Silicon Debug Layer1 Status Lane 2 ...
	p += snprintf(p, end - p, " %s", disp_ampereone_payload1_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%lx\n",
		     (unsigned long)err->dbg_l1_status_lane2);

	//display Silicon Debug Layer1 Status Lane 3 ...
	p += snprintf(p, end - p, " %s", disp_ampereone_payload1_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%lx\n",
		     (unsigned long)err->dbg_l1_status_lane3);

	//display Silicon Debug Layer1 Status Lane 4 ...
	p += snprintf(p, end - p, " %s", disp_ampereone_payload1_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%lx\n",
		     (unsigned long)err->dbg_l1_status_lane4);

	//display Silicon Debug Layer1 Status Lane 5 ...
	p += snprintf(p, end - p, " %s", disp_ampereone_payload1_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%lx\n",
		     (unsigned long)err->dbg_l1_status_lane5);

	//display Silicon Debug Layer1 Status Lane 6 ...
	p += snprintf(p, end - p, " %s", disp_ampereone_payload1_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%lx\n",
		     (unsigned long)err->dbg_l1_status_lane6);

	//display Silicon Debug Layer1 Status Lane 7 ...
	p += snprintf(p, end - p, " %s", disp_ampereone_payload1_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%lx\n",
		     (unsigned long)err->dbg_l1_status_lane7);

	//display Silicon Debug Layer1 Status Lane 8 ...
	p += snprintf(p, end - p, " %s", disp_ampereone_payload1_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%lx\n",
		     (unsigned long)err->dbg_l1_status_lane8);

	//display Silicon Debug Layer1 Status Lane 9 ...
	p += snprintf(p, end - p, " %s", disp_ampereone_payload1_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%lx\n",
		     (unsigned long)err->dbg_l1_status_lane9);

	//display Silicon Debug Layer1 Status Lane 10 ...
	p += snprintf(p, end - p, " %s", disp_ampereone_payload1_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%lx\n",
		     (unsigned long)err->dbg_l1_status_lane10);

	//display Silicon Debug Layer1 Status Lane 11 ...
	p += snprintf(p, end - p, " %s", disp_ampereone_payload1_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%lx\n",
		     (unsigned long)err->dbg_l1_status_lane11);

	//display Silicon Debug Layer1 Status Lane 12 ...
	p += snprintf(p, end - p, " %s", disp_ampereone_payload1_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%lx\n",
		     (unsigned long)err->dbg_l1_status_lane12);

	//display Silicon Debug Layer1 Status Lane 13 ...
	p += snprintf(p, end - p, " %s", disp_ampereone_payload1_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%lx\n",
		     (unsigned long)err->dbg_l1_status_lane13);

	//display Silicon Debug Layer1 Status Lane 14 ...
	p += snprintf(p, end - p, " %s", disp_ampereone_payload1_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%lx\n",
		     (unsigned long)err->dbg_l1_status_lane14);

	//display Silicon Debug Layer1 Status Lane 15 ...
	p += snprintf(p, end - p, " %s", disp_ampereone_payload1_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%lx\n",
		     (unsigned long)err->dbg_l1_status_lane15);

	if (p > buf && p < end) {
		p--;
		*p = '\0';
	}

	trace_seq_printf(s, "%s\n", buf);
	i = 0;
	p = NULL;
	end = NULL;

	record_ampereone_payload1_err(ev_decoder, type_str, subtype_str, err);

}

// Payload Type 2: PCIe RASDP
void decode_ampereone_payload2_err_regs(struct ras_ns_ev_decoder *ev_decoder,
				struct trace_seq *s,
				const struct ampereone_payload2_type_sec *err)
{
	char buf[AMPEREONE_PAYLOAD2_BUF_LEN];
	char *p = buf;
	char *end = buf + AMPEREONE_PAYLOAD2_BUF_LEN;
	int i = 0;
	const char *subtype_str;

	const char *type_str = ampereone_type_name(ampereone_payload_error_type,
					    AMPEREONE_TYPE(err->header.type));


	subtype_str  = ampereone_subtype_name(ampereone_payload_error_type,
					    AMPEREONE_TYPE(err->header.type), err->header.subtype);

	trace_seq_printf(s, " Payload Type: 2\n");

	// Display error type
	p += snprintf(p, end - p, " %s", disp_ampereone_payload2_err_reg_name[i++]);
	p += snprintf(p, end - p, " %s\n", type_str);

	// Display error subtype
	p += snprintf(p, end - p, " %s", disp_ampereone_payload2_err_reg_name[i++]);
	p += snprintf(p, end - p, " %s\n", subtype_str);

	// Display error instance
	p += snprintf(p, end - p, " %s", disp_ampereone_payload2_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%x\n", AMPEREONE_INSTANCE(err->header.instance));

	// Display socket number
	p += snprintf(p, end - p, " %s", disp_ampereone_payload2_err_reg_name[i++]);
	p += snprintf(p, end - p, " %d\n", AMPEREONE_SOCKET_NUM(err->header.instance));

	//display Correctable Count
	p += snprintf(p, end - p, " %s", disp_ampereone_payload2_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%lx\n",
		     (unsigned long)err->corr_count_report);

	//display Correctable Error Location
	p += snprintf(p, end - p, " %s", disp_ampereone_payload2_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%lx\n",
		     (unsigned long)err->corr_error_location);

	//display RAM Address Correctable
	p += snprintf(p, end - p, " %s", disp_ampereone_payload2_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%lx\n",
		     (unsigned long)err->ram_addr_corr);

	//display Uncorrectable Count
	p += snprintf(p, end - p, " %s", disp_ampereone_payload2_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%lx\n",
		     (unsigned long)err->uncorr_count_report);

	//display Uncorrectable Error Location
	p += snprintf(p, end - p, " %s", disp_ampereone_payload2_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%lx\n",
		     (unsigned long)err->uncorr_error_location);

	//display RAM Addr Uncorrectable
	p += snprintf(p, end - p, " %s", disp_ampereone_payload2_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%lx\n",
		     (unsigned long)err->ram_addr_uncorr);

	if (p > buf && p < end) {
		p--;
		*p = '\0';
	}

	record_ampereone_payload2_err(ev_decoder, type_str, subtype_str, err);
	i = 0;
	p = NULL;
	end = NULL;
	trace_seq_printf(s, "%s\n", buf);
}

void decode_ampereone_payload3_err_regs(struct ras_ns_ev_decoder *ev_decoder,
				struct trace_seq *s,
				const struct ampereone_payload3_type_sec *err)
{
	char buf[AMPEREONE_PAYLOAD3_BUF_LEN];
	char *p = buf;
	char *end = buf + AMPEREONE_PAYLOAD3_BUF_LEN;
	int i = 0;
	const char *subtype_str;

	const char *type_str = ampereone_type_name(ampereone_payload_error_type,
					    AMPEREONE_TYPE(err->header.type));


	subtype_str  = ampereone_subtype_name(ampereone_payload_error_type,
					    AMPEREONE_TYPE(err->header.type), err->header.subtype);

	trace_seq_printf(s, " Payload Type: 3\n");

	// Display error type
	p += snprintf(p, end - p, " %s", disp_ampereone_payload3_err_reg_name[i++]);
	p += snprintf(p, end - p, " %s\n", type_str);

	// Display error subtype
	p += snprintf(p, end - p, " %s", disp_ampereone_payload3_err_reg_name[i++]);
	p += snprintf(p, end - p, " %s\n", subtype_str);

	// Display error instance
	p += snprintf(p, end - p, " %s", disp_ampereone_payload3_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%x\n", AMPEREONE_INSTANCE(err->header.instance));

	// Display socket number
	p += snprintf(p, end - p, " %s", disp_ampereone_payload3_err_reg_name[i++]);
	p += snprintf(p, end - p, " %d\n", AMPEREONE_SOCKET_NUM(err->header.instance));

	// Display ECCxAddr
	p += snprintf(p, end - p, " %s", disp_ampereone_payload3_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%llx\n",
		     (unsigned long long)err->ecc_addr);

	// Display ECCxData
	p += snprintf(p, end - p, " %s", disp_ampereone_payload3_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%llx\n",
		     (unsigned long long)err->ecc_data);

	// Display ECCxSrcId
	p += snprintf(p, end - p, " %s", disp_ampereone_payload3_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%lx\n",
		     (unsigned long)err->ecc_id);

	// Display ECCxSynd
	p += snprintf(p, end - p, " %s", disp_ampereone_payload3_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%lx\n",
		     (unsigned long)err->ecc_synd);

	// Display ECCxMceCnt
	p += snprintf(p, end - p, " %s", disp_ampereone_payload3_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%lx\n",
		     (unsigned long)err->ecc_mce_cnt);

	// Display ECCxCtlr
	p += snprintf(p, end - p, " %s", disp_ampereone_payload3_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%lx\n",
		     (unsigned long)err->ecc_ctlr);

	// Display ECCxErrSts
	p += snprintf(p, end - p, " %s", disp_ampereone_payload3_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%lx\n",
		     (unsigned long)err->ecc_err_sts);

	// Display ECCxErrCnt
	p += snprintf(p, end - p, " %s", disp_ampereone_payload3_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%lx\n",
		     (unsigned long)err->ecc_err_cnt);

	if (p > buf && p < end) {
		p--;
		*p = '\0';
	}

	record_ampereone_payload3_err(ev_decoder, type_str, subtype_str, err);
	i = 0;
	p = NULL;
	end = NULL;
	trace_seq_printf(s, "%s\n", buf);
}

void decode_ampereone_payload4_err_regs(struct ras_ns_ev_decoder *ev_decoder,
				struct trace_seq *s,
				const struct ampereone_payload4_type_sec *err)
{
	char buf[AMPEREONE_PAYLOAD4_BUF_LEN];
	char *p = buf;
	char *end = buf + AMPEREONE_PAYLOAD4_BUF_LEN;
	int i = 0;
	const char *subtype_str;

	const char *type_str = ampereone_type_name(ampereone_payload_error_type,
					    AMPEREONE_TYPE(err->header.type));


	subtype_str  = ampereone_subtype_name(ampereone_payload_error_type,
					    AMPEREONE_TYPE(err->header.type), err->header.subtype);

	trace_seq_printf(s, " Payload Type: 4\n");

	// Display error type
	p += snprintf(p, end - p, " %s", disp_ampereone_payload4_err_reg_name[i++]);
	p += snprintf(p, end - p, " %s\n", type_str);

	// Display error subtype
	p += snprintf(p, end - p, " %s", disp_ampereone_payload4_err_reg_name[i++]);
	p += snprintf(p, end - p, " %s\n", subtype_str);

	// Display error instance
	p += snprintf(p, end - p, " %s", disp_ampereone_payload4_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%x\n", AMPEREONE_INSTANCE(err->header.instance));

	// Display socket number
	p += snprintf(p, end - p, " %s", disp_ampereone_payload4_err_reg_name[i++]);
	p += snprintf(p, end - p, " %d\n", AMPEREONE_SOCKET_NUM(err->header.instance));

	// Display Address
	p += snprintf(p, end - p, " %s", disp_ampereone_payload4_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%llx\n",
		     (unsigned long long)err->address);

	// Display Src Id
	p += snprintf(p, end - p, " %s", disp_ampereone_payload4_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%lx\n",
		     (unsigned long)err->srcid);

	// Display Txn Id
	p += snprintf(p, end - p, " %s", disp_ampereone_payload4_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%lx\n",
		     (unsigned long)err->txnid);

	// Display Type
	p += snprintf(p, end - p, " %s", disp_ampereone_payload4_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%lx\n",
		     (unsigned long)err->type);

	// Display LP Id
	p += snprintf(p, end - p, " %s", disp_ampereone_payload4_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%lx\n",
		     (unsigned long)err->lpid);

	// Display Opcode
	p += snprintf(p, end - p, " %s", disp_ampereone_payload4_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%lx\n",
		     (unsigned long)err->opcode);

	// Display tag
	p += snprintf(p, end - p, " %s", disp_ampereone_payload4_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%lx\n",
		     (unsigned long)err->tag);

	// Display MPAM
	p += snprintf(p, end - p, " %s", disp_ampereone_payload4_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%lx\n",
		     (unsigned long)err->mpam);

	if (p > buf && p < end) {
		p--;
		*p = '\0';
	}

	record_ampereone_payload4_err(ev_decoder, type_str, subtype_str, err);
	i = 0;
	p = NULL;
	end = NULL;
	trace_seq_printf(s, "%s\n", buf);
}

void decode_ampereone_payload5_err_regs(struct ras_ns_ev_decoder *ev_decoder,
				struct trace_seq *s,
				const struct ampereone_payload5_type_sec *err)
{
	char buf[AMPEREONE_PAYLOAD5_BUF_LEN];
	char *p = buf;
	char *end = buf + AMPEREONE_PAYLOAD5_BUF_LEN;
	int i = 0;
	const char *subtype_str;

	const char *type_str = ampereone_type_name(ampereone_payload_error_type,
					    AMPEREONE_TYPE(err->header.type));


	subtype_str  = ampereone_subtype_name(ampereone_payload_error_type,
					    AMPEREONE_TYPE(err->header.type), err->header.subtype);

	trace_seq_printf(s, " Payload Type: 5\n");

	//display error type
	p += snprintf(p, end - p, " %s", disp_ampereone_payload5_err_reg_name[i++]);
	p += snprintf(p, end - p, " %s\n", type_str);

	//display error subtype
	p += snprintf(p, end - p, " %s", disp_ampereone_payload5_err_reg_name[i++]);
	p += snprintf(p, end - p, " %s\n", subtype_str);

	//display error instance
	p += snprintf(p, end - p, " %s", disp_ampereone_payload5_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%x\n", AMPEREONE_INSTANCE(err->header.instance));

	//display socket number
	p += snprintf(p, end - p, " %s", disp_ampereone_payload5_err_reg_name[i++]);
	p += snprintf(p, end - p, " %d\n", AMPEREONE_SOCKET_NUM(err->header.instance));

	if (p > buf && p < end) {
		p--;
		*p = '\0';
	}

	record_ampereone_payload5_err(ev_decoder, type_str, subtype_str, err);
	i = 0;
	p = NULL;
	end = NULL;
	trace_seq_printf(s, "%s\n", buf);
}

void decode_ampereone_payload6_err_regs(struct ras_ns_ev_decoder *ev_decoder,
				struct trace_seq *s,
				const struct ampereone_payload6_type_sec *err)
{
	char buf[AMPEREONE_PAYLOAD6_BUF_LEN];
	char *p = buf;
	char *end = buf + AMPEREONE_PAYLOAD6_BUF_LEN;
	int i = 0;
	const char *subtype_str;

	const char *type_str = ampereone_type_name(ampereone_payload_error_type,
					    AMPEREONE_TYPE(err->header.type));


	subtype_str  = ampereone_subtype_name(ampereone_payload_error_type,
					    AMPEREONE_TYPE(err->header.type), err->header.subtype);

	trace_seq_printf(s, " Payload Type: 6\n");

	//display error type
	p += snprintf(p, end - p, " %s", disp_ampereone_payload6_err_reg_name[i++]);
	p += snprintf(p, end - p, " %s\n", type_str);

	//display error subtype
	p += snprintf(p, end - p, " %s", disp_ampereone_payload6_err_reg_name[i++]);
	p += snprintf(p, end - p, " %s\n", subtype_str);

	//display error instance
	p += snprintf(p, end - p, " %s", disp_ampereone_payload6_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%x\n", AMPEREONE_INSTANCE(err->header.instance));

	//display socket number
	p += snprintf(p, end - p, " %s", disp_ampereone_payload6_err_reg_name[i++]);
	p += snprintf(p, end - p, " %d\n", AMPEREONE_SOCKET_NUM(err->header.instance));

	// Display Driver
	p += snprintf(p, end - p, " %s", disp_ampereone_payload6_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%x\n",
		     (uint8_t)err->driver);

	// Display Error Code
	p += snprintf(p, end - p, " %s", disp_ampereone_payload6_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%lx\n",
		     (unsigned long)err->error_code);

	// Display Error Msg. Size
	p += snprintf(p, end - p, " %s", disp_ampereone_payload6_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%x\n",
		     (uint8_t)err->error_msg_size);

	// Display Error Msg in a hex representation.
	display_hex_dump(&p, end, err->error_msg_size, err->error_msg,
					 disp_ampereone_payload6_err_reg_name[i++]);

	if (p > buf && p < end) {
		p--;
		*p = '\0';
	}

	record_ampereone_payload6_err(ev_decoder, type_str, subtype_str, err);
	i = 0;
	p = NULL;
	end = NULL;
	trace_seq_printf(s, "%s\n", buf);
}

/* error data decoding functions */
int decode_ampereone_type_error(struct ras_events *ras,
				     struct ras_ns_ev_decoder *ev_decoder,
				     struct trace_seq *s,
				     struct ras_non_standard_event *event)
{
	int payload_type = AMPEREONE_PAYLOAD_TYPE(
		((struct ampereone_payload_header *)event->error)->type);

#ifdef HAVE_SQLITE3
	struct db_table_descriptor db_tab;
	int id = 0;

	if (payload_type == AMPEREONE_PAYLOAD_TYPE_0) {
		db_tab = ampereone_payload0_event_tab;
		id = AMPEREONE_PAYLOAD0_FIELD_TIMESTAMP;
	} else if (payload_type == AMPEREONE_PAYLOAD_TYPE_1) {
		db_tab = ampereone_payload1_event_tab;
		id = AMPEREONE_PAYLOAD1_FIELD_TIMESTAMP;
	} else if (payload_type == AMPEREONE_PAYLOAD_TYPE_2) {
		db_tab = ampereone_payload2_event_tab;
		id = AMPEREONE_PAYLOAD2_FIELD_TIMESTAMP;
	} else if (payload_type == AMPEREONE_PAYLOAD_TYPE_3) {
		db_tab = ampereone_payload3_event_tab;
		id = AMPEREONE_PAYLOAD3_FIELD_TIMESTAMP;
	} else if (payload_type == AMPEREONE_PAYLOAD_TYPE_4) {
		db_tab = ampereone_payload4_event_tab;
		id = AMPEREONE_PAYLOAD4_FIELD_TIMESTAMP;
	} else if (payload_type == AMPEREONE_PAYLOAD_TYPE_5) {
		db_tab = ampereone_payload5_event_tab;
		id = AMPEREONE_PAYLOAD5_FIELD_TIMESTAMP;
	} else if (payload_type == AMPEREONE_PAYLOAD_TYPE_6) {
		db_tab = ampereone_payload6_event_tab;
		id = AMPEREONE_PAYLOAD6_FIELD_TIMESTAMP;
	} else {
		return -1;
	}

	if (ras->record_events) {
		if (ras_mc_add_vendor_table(ras, &ev_decoder->stmt_dec_record,
					    &db_tab) != SQLITE_OK) {
			trace_seq_printf(s,
					 "create sql %s fail\n",
					 sqlite3_table_list[payload_type]);
			return -1;
		}
	}
	record_ampereone_data(ev_decoder, AMPEREONE_OEM_DATA_TYPE_TEXT,
			id, 0, event->timestamp);
#endif

	if (payload_type == AMPEREONE_PAYLOAD_TYPE_0) {
		const struct ampereone_payload0_type_sec *err =
			(struct ampereone_payload0_type_sec *)event->error;
		decode_ampereone_payload0_err_regs(ev_decoder, s, err);
	} else if (payload_type == AMPEREONE_PAYLOAD_TYPE_1) {
		const struct ampereone_payload1_type_sec *err =
			(struct ampereone_payload1_type_sec *)event->error;
		decode_ampereone_payload1_err_regs(ev_decoder, s, err);
	} else if (payload_type == AMPEREONE_PAYLOAD_TYPE_2) {
		const struct ampereone_payload2_type_sec *err =
			(struct ampereone_payload2_type_sec *)event->error;
		decode_ampereone_payload2_err_regs(ev_decoder, s, err);
	} else if (payload_type == AMPEREONE_PAYLOAD_TYPE_3) {
		const struct ampereone_payload3_type_sec *err =
			(struct ampereone_payload3_type_sec *)event->error;
		decode_ampereone_payload3_err_regs(ev_decoder, s, err);
	} else if (payload_type == AMPEREONE_PAYLOAD_TYPE_4) {
		const struct ampereone_payload4_type_sec *err =
			(struct ampereone_payload4_type_sec *)event->error;
		decode_ampereone_payload4_err_regs(ev_decoder, s, err);
	} else if (payload_type == AMPEREONE_PAYLOAD_TYPE_5) {
		const struct ampereone_payload5_type_sec *err =
			(struct ampereone_payload5_type_sec *)event->error;
		decode_ampereone_payload5_err_regs(ev_decoder, s, err);
	} else if (payload_type == AMPEREONE_PAYLOAD_TYPE_6) {
		const struct ampereone_payload6_type_sec *err =
			(struct ampereone_payload6_type_sec *)event->error;
		decode_ampereone_payload6_err_regs(ev_decoder, s, err);
	} else {
		trace_seq_printf(s, "%s: unknown payload type\n", __func__);
		return -1;
	}
	return 0;
}

struct ras_ns_ev_decoder ampereone_ns_oem_decoder[] = {
	{
		.sec_type = "2826cc9f-448c-4c2b-86b6-a95394b7ef33",
		.decode = decode_ampereone_type_error,
	},
};

static void __attribute__((constructor)) ampereone_init(void)
{
	register_ns_ev_decoder(ampereone_ns_oem_decoder);
}
