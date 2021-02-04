/*
 * Copyright (c) 2020, Ampere Computing LLC.
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
#include <stdbool.h>
#include "ras-record.h"
#include "ras-logger.h"
#include "ras-report.h"
#include "ras-non-standard-handler.h"
#include "non-standard-ampere.h"

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
		if (type->sub == NULL)
			return type->name;
		if (sub_type_id >= type->sub_num)
			return "unknown";
		return submodule[sub_type_id];
	}
	return "unknown";
}


/*decode ampere specific error payload type 0, the CPU's data is save*/
/*to sqlite by ras-arm-handler, others are saved by this function.*/
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
	if ((TYPE(err->type) == 0) &&
	    ((err->subtype == 0x01) || (err->subtype == 0x02))) {
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

	i = 0;
	p = NULL;
	end = NULL;
	trace_seq_printf(s, "%s\n", buf);
}

/*decode ampere specific error payload type 1 and save to sqlite db*/
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

	i = 0;
	p = NULL;
	end = NULL;
	trace_seq_printf(s, "%s\n", buf);
}

/*decode ampere specific error payload type 2 and save to sqlite db*/
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

	i = 0;
	p = NULL;
	end = NULL;
	trace_seq_printf(s, "%s\n", buf);
}

/*decode ampere specific error payload type 3 and save to sqlite db*/
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
	int payload_type = PAYLOAD_TYPE(event->error[0]);

	if (payload_type == PAYLOAD_TYPE_0) {
		const struct amp_payload0_type_sec *err =
			(struct amp_payload0_type_sec *)event->error;
		decode_amp_payload0_err_regs(ev_decoder, s, err);

	} else if (payload_type == PAYLOAD_TYPE_1) {
		const struct amp_payload1_type_sec *err =
			(struct amp_payload1_type_sec *)event->error;
		decode_amp_payload1_err_regs(ev_decoder, s, err);
	} else if (payload_type == PAYLOAD_TYPE_2) {
		const struct amp_payload2_type_sec *err =
			(struct amp_payload2_type_sec *)event->error;
		decode_amp_payload2_err_regs(ev_decoder, s, err);
	} else if (payload_type == PAYLOAD_TYPE_3) {
		const struct amp_payload3_type_sec *err =
			(struct amp_payload3_type_sec *)event->error;
		decode_amp_payload3_err_regs(ev_decoder, s, err);
	} else {
		trace_seq_printf(s, "%s: wrong payload type\n", __func__);
		return -1;
	}
	return 0;
}

struct ras_ns_ev_decoder amp_ns_oem_decoder[] = {
	{
		.sec_type = "e8ed898ddf1643cc8ecc54f060ef157f",
		.decode = decode_amp_oem_type_error,
	},
};

static void __attribute__((constructor)) amp_init(void)
{
	register_ns_ev_decoder(amp_ns_oem_decoder);
}
