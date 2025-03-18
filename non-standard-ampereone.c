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
							 const char * const *disp_reg_name, int i);

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
	"Silicon Debug Layer 1 Status Lane 0",
	"Silicon Debug Layer 1 Status Lane 1",
	"Silicon Debug Layer 1 Status Lane 2",
	"Silicon Debug Layer 1 Status Lane 3",
	"Silicon Debug Layer 1 Status Lane 4",
	"Silicon Debug Layer 1 Status Lane 5",
	"Silicon Debug Layer 1 Status Lane 6",
	"Silicon Debug Layer 1 Status Lane 7",
	"Silicon Debug Layer 1 Status Lane 8",
	"Silicon Debug Layer 1 Status Lane 9",
	"Silicon Debug Layer 1 Status Lane 10",
	"Silicon Debug Layer 1 Status Lane 11",
	"Silicon Debug Layer 1 Status Lane 12",
	"Silicon Debug Layer 1 Status Lane 13",
	"Silicon Debug Layer 1 Status Lane 14",
	"Silicon Debug Layer 1 Status Lane 15",
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
					  const char * const *disp_reg_name, int i)
{
	for (uint8_t c = 0; c < size; c++) {

		if (c == 0)
			*p += snprintf(*p, end - *p, " %s\n", disp_reg_name[i++]);
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

	//record_ampereone_payload0_err(ev_decoder, type_str, subtype_str, err);
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

	//display Silicon Debug Layer1 Status Lane 0 ...
	p += snprintf(p, end - p, " %s", disp_ampereone_payload1_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%lx\n",
		     (unsigned long)err->dbg_l1_status_lane0);

	//display Silicon Debug Layer1 Status Lane 1 ...
	p += snprintf(p, end - p, " %s", disp_ampereone_payload1_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%lx\n",
		     (unsigned long)err->dbg_l1_status_lane1);

	//display Silicon Debug Layer1 Status Lane 0 ...
	p += snprintf(p, end - p, " %s", disp_ampereone_payload1_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%lx\n",
		     (unsigned long)err->dbg_l1_status_lane2);

	//display Silicon Debug Layer1 Status Lane 0 ...
	p += snprintf(p, end - p, " %s", disp_ampereone_payload1_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%lx\n",
		     (unsigned long)err->dbg_l1_status_lane3);

	//display Silicon Debug Layer1 Status Lane 0 ...
	p += snprintf(p, end - p, " %s", disp_ampereone_payload1_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%lx\n",
		     (unsigned long)err->dbg_l1_status_lane4);

	//display Silicon Debug Layer1 Status Lane 0 ...
	p += snprintf(p, end - p, " %s", disp_ampereone_payload1_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%lx\n",
		     (unsigned long)err->dbg_l1_status_lane5);

	//display Silicon Debug Layer1 Status Lane 0 ...
	p += snprintf(p, end - p, " %s", disp_ampereone_payload1_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%lx\n",
		     (unsigned long)err->dbg_l1_status_lane6);

	//display Silicon Debug Layer1 Status Lane 0 ...
	p += snprintf(p, end - p, " %s", disp_ampereone_payload1_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%lx\n",
		     (unsigned long)err->dbg_l1_status_lane7);

	//display Silicon Debug Layer1 Status Lane 0 ...
	p += snprintf(p, end - p, " %s", disp_ampereone_payload1_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%lx\n",
		     (unsigned long)err->dbg_l1_status_lane8);

	//display Silicon Debug Layer1 Status Lane 0 ...
	p += snprintf(p, end - p, " %s", disp_ampereone_payload1_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%lx\n",
		     (unsigned long)err->dbg_l1_status_lane9);

	//display Silicon Debug Layer1 Status Lane 0 ...
	p += snprintf(p, end - p, " %s", disp_ampereone_payload1_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%lx\n",
		     (unsigned long)err->dbg_l1_status_lane10);

	//display Silicon Debug Layer1 Status Lane 0 ...
	p += snprintf(p, end - p, " %s", disp_ampereone_payload1_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%lx\n",
		     (unsigned long)err->dbg_l1_status_lane11);

	//display Silicon Debug Layer1 Status Lane 0 ...
	p += snprintf(p, end - p, " %s", disp_ampereone_payload1_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%lx\n",
		     (unsigned long)err->dbg_l1_status_lane12);

	//display Silicon Debug Layer1 Status Lane 0 ...
	p += snprintf(p, end - p, " %s", disp_ampereone_payload1_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%lx\n",
		     (unsigned long)err->dbg_l1_status_lane13);

	//display Silicon Debug Layer1 Status Lane 0 ...
	p += snprintf(p, end - p, " %s", disp_ampereone_payload1_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%lx\n",
		     (unsigned long)err->dbg_l1_status_lane14);

	//display Silicon Debug Layer1 Status Lane 0 ...
	p += snprintf(p, end - p, " %s", disp_ampereone_payload1_err_reg_name[i++]);
	p += snprintf(p, end - p, " 0x%lx\n",
		     (unsigned long)err->dbg_l1_status_lane15);

	if (p > buf && p < end) {
		p--;
		*p = '\0';
	}

	//record_ampereone_payload1_err(ev_decoder, type_str, subtype_str, err);
	i = 0;
	p = NULL;
	end = NULL;
	trace_seq_printf(s, "%s\n", buf);
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

	//record_ampereone_payload2_err(ev_decoder, type_str, subtype_str, err);
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

	//record_ampereone_payload3_err(ev_decoder, type_str, subtype_str, err);
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

	//record_ampereone_payload4_err(ev_decoder, type_str, subtype_str, err);
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

	//record_ampereone_payload5_err(ev_decoder, type_str, subtype_str, err);
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
	display_hex_dump(&p, end, 61 /*err->error_msg_size*/, err->error_msg,
					 disp_ampereone_payload6_err_reg_name, i);

	if (p > buf && p < end) {
		p--;
		*p = '\0';
	}

	//record_ampereone_payload6_err(ev_decoder, type_str, subtype_str, err);
	i = 0;
	p = NULL;
	end = NULL;
	trace_seq_printf(s, "%s\n", buf);
}

/* error data decoding functions */
static int decode_ampereone_type_error(struct ras_events *ras,
				     struct ras_ns_ev_decoder *ev_decoder,
				     struct trace_seq *s,
				     struct ras_non_standard_event *event)
{
	int payload_type = AMPEREONE_PAYLOAD_TYPE(
		((struct ampereone_payload_header *)event->error)->type);

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
