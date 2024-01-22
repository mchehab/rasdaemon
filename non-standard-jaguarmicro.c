/*
 * Copyright (c) 2023, JaguarMicro
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
#include "non-standard-jaguarmicro.h"
#include "ras-mce-handler.h"

#define JM_BUF_LEN	256
#define JM_REG_BUF_LEN	2048
#define JM_SNPRINTF	mce_snprintf

static void record_jm_data(struct ras_ns_ev_decoder *ev_decoder,
			   enum jm_oem_data_type data_type,
				int id, int64_t data, const char *text);

struct jm_event {
	char error_msg[JM_BUF_LEN];
	char reg_msg[JM_REG_BUF_LEN];
};

/*ras_csr_por Payload Type 0*/
static const char * const disp_payload0_err_reg_name[] = {
	"LOCK_CONTROL:",
	"LOCK_FUNCTION:",
	"CFG_RAM_ID:",
	"ERR_FR_LOW32:",
	"ERR_FR_HIGH32:",
	"ERR_CTLR_LOW32:",
	"ECC_STATUS_LOW32:",
	"ECC_ADDR_LOW32:",
	"ECC_ADDR_HIGH32:",
	"ECC_MISC0_LOW32:",
	"ECC_MISC0_HIGH32:",
	"ECC_MISC1_LOW32:",
	"ECC_MISC1_HIGH32:",
	"ECC_MISC2_LOW32:",
	"ECC_MISC2_HIGH32:",
};

/*SMMU IP Payload Type 1*/
static const char * const disp_payload1_err_reg_name[] = {
	"CSR_INT_STATUS:",
	"ERR_FR:",
	"ERR_CTLR:",
	"ERR_STATUS:",
	"ERR_GEN:",
};

/*HAC SRAM, Payload Type 2 */
static const char * const disp_payload2_err_reg_name[] = {
	"ECC_1BIT_INFO_LOW32:",
	"ECC_1BIT_INFO_HIGH32:",
	"ECC_2BIT_INFO_LOW32:",
	"ECC_2BIT_INFO_HIGH32:",
};

/*CMN IP, Payload Type 5 */
static const char * const disp_payload5_err_reg_name[] = {
	"CFGM_MXP_0:",
	"CFGM_HNF_0:",
	"CFGM_HNI_0:",
	"CFGM_SBSX_0:",
	"ERR_FR_NS:",
	"ERR_CTLRR_NS:",
	"ERR_STATUSR_NS:",
	"ERR_ADDRR_NS:",
	"ERR_MISCR_NS:",
	"ERR_FR:",
	"ERR_CTLR:",
	"ERR_STATUS:",
	"ERR_ADDR:",
	"ERR_MISC:",
};

/*GIC IP, Payload Type 6 */
static const char * const disp_payload6_err_reg_name[] = {
	"RECORD_ID:",
	"GICT_ERR_FR:",
	"GICT_ERR_CTLR:",
	"GICT_ERR_STATUS:",
	"GICT_ERR_ADDR:",
	"GICT_ERR_MISC0:",
	"GICT_ERR_MISC1:",
	"GICT_ERRGSR:",
};

static const char * const soc_desc[] = {
	"Corsica1.0",
};

/* JaguarMicro sub system definitions */
#define JM_SUB_SYS_CSUB		0
#define JM_SUB_SYS_CMN		1
#define JM_SUB_SYS_DDRH		2
#define JM_SUB_SYS_DDRV		3
#define JM_SUB_SYS_GIC		4
#define JM_SUB_SYS_IOSUB	5
#define JM_SUB_SYS_SCP		6
#define JM_SUB_SYS_MCP		7
#define JM_SUB_SYS_IMU0		8
#define JM_SUB_SYS_DPE		9
#define JM_SUB_SYS_RPE		10
#define JM_SUB_SYS_PSUB		11
#define JM_SUB_SYS_HAC		12
#define JM_SUB_SYS_TCM		13
#define JM_SUB_SYS_IMU1		14

static const char * const subsystem_desc[] = {
	"N2",
	"CMN",
	"DDRH",
	"DDRV",
	"GIC",
	"IOSUB",
	"SCP",
	"MCP",
	"IMU0",
	"DPE",
	"RPE",
	"PSUB",
	"HAC",
	"TCM",
	"IMU1",
};

static const char * const cmn_module_desc[] = {
	"MXP",
	"HNI",
	"HNF",
	"SBSX",
	"CCG",
	"HND",
};

static const char * const ddr_module_desc[] = {
	"DDRCtrl",
	"DDRPHY",
	"SRAM",
};

static const char * const gic_module_desc[] = {
	"GICIP",
	"GICSRAM",
};

/* JaguarMicro IOSUB sub system module definitions */
#define JM_SUBSYS_IOSUB_MOD_SMMU		0
#define JM_SUBSYS_IOSUB_MOD_NIC450		1
#define JM_SUBSYS_IOSUB_MOD_OTHER		2

static const char * const iosub_module_desc[] = {
	"SMMU",
	"NIC450",
	"OTHER",
};

static const char * const scp_module_desc[] = {
	"SRAM",
	"WDT",
	"PLL",
};

static const char * const mcp_module_desc[] = {
	"SRAM",
	"WDT",
};

static const char * const imu_module_desc[] = {
	"SRAM",
	"WDT",
};

/* JaguarMicro DPE sub system module definitions */
#define JM_SUBSYS_DPE_MOD_EPG		0
#define JM_SUBSYS_DPE_MOD_PIPE		1
#define JM_SUBSYS_DPE_MOD_EMEP		2
#define JM_SUBSYS_DPE_MOD_IMEP		3
#define JM_SUBSYS_DPE_MOD_EPAE		4
#define JM_SUBSYS_DPE_MOD_IPAE		5
#define JM_SUBSYS_DPE_MOD_ETH		6
#define JM_SUBSYS_DPE_MOD_TPG		7
#define JM_SUBSYS_DPE_MOD_MIG		8
#define JM_SUBSYS_DPE_MOD_HIG		9
#define JM_SUBSYS_DPE_MOD_DPETOP	10
#define JM_SUBSYS_DPE_MOD_SMMU		11

static const char * const dpe_module_desc[] = {
	"EPG",
	"PIPE",
	"EMEP",
	"IMEP",
	"EPAE",
	"IPAE",
	"ETH",
	"TPG",
	"MIG",
	"HIG",
	"DPETOP",
	"SMMU",
};

/* JaguarMicro RPE sub system module definitions */
#define JM_SUBSYS_RPE_MOD_TOP		0
#define JM_SUBSYS_RPE_MOD_TXP_RXP	1
#define JM_SUBSYS_RPE_MOD_SMMU		2

static const char * const rpe_module_desc[] = {
	"TOP",
	"TXP_RXP",
	"SMMU",
};

/* JaguarMicro PSUB sub system module definitions */
#define JM_SUBSYS_PSUB_MOD_PCIE0		0
#define JM_SUBSYS_PSUB_MOD_UP_MIX		1
#define JM_SUBSYS_PSUB_MOD_PCIE1		2
#define JM_SUBSYS_PSUB_MOD_PTOP			3
#define JM_SUBSYS_PSUB_MOD_N2IF			4
#define JM_SUBSYS_PSUB_MOD_VPE0_RAS		5
#define JM_SUBSYS_PSUB_MOD_VPE1_RAS		6
#define JM_SUBSYS_PSUB_MOD_X2RC_SMMU	7
#define JM_SUBSYS_PSUB_MOD_X16RC_SMMU	8
#define JM_SUBSYS_PSUB_MOD_SDMA_SMMU	9

static const char * const psub_module_desc[] = {
	"PCIE0",
	"UP_MIX",
	"PCIE1",
	"PTOP",
	"N2IF",
	"VPE0_RAS",
	"VPE1_RAS",
	"X2RC_SMMU",
	"X16RC_SMMU",
	"SDMA_SMMU",
};

static const char * const hac_module_desc[] = {
	"SRAM",
	"SMMU",
};

#define JM_SUBSYS_TCM_MOD_SRAM		0
#define JM_SUBSYS_TCM_MOD_SMMU		1
#define JM_SUBSYS_TCM_MOD_IP		2

static const char * const tcm_module_desc[] = {
	"SRAM",
	"SMMU",
	"IP",
};

static const char * const iosub_smmu_sub_desc[] = {
	"TBU",
	"TCU",
};

static const char * const iosub_other_sub_desc[] = {
	"RAM",
};

static const char * const smmu_sub_desc[] = {
	"TCU",
	"TBU",
};

static const char * const psub_pcie0_sub_desc[] = {
	"RAS0",
	"RAS1",
};

static const char * const csub_dev_desc[] = {
	"CORE",
};

static const char * const cmn_dev_desc[] = {
	"NID",
};

static const char * const ddr_dev_desc[] = {
	"CHNL",
};

static const char * const default_dev_desc[] = {
	"DEV",
};

static const char *get_jm_soc_desc(uint8_t soc_id)
{
	if (soc_id >= sizeof(soc_desc) / sizeof(char *))
		return "unknown";

	return soc_desc[soc_id];
}

static const char *get_jm_subsystem_desc(uint8_t subsys_id)
{
	if (subsys_id >= sizeof(subsystem_desc) / sizeof(char *))
		return "unknown";

	return subsystem_desc[subsys_id];
}

static const char *get_jm_module_desc(uint8_t subsys_id, uint8_t mod_id)
{
	const char * const*module;
	int tbl_size;

	switch (subsys_id) {
	case JM_SUB_SYS_CMN:
		module = cmn_module_desc;
		tbl_size = sizeof(cmn_module_desc) / sizeof(char *);
		break;
	case JM_SUB_SYS_DDRH:
	case JM_SUB_SYS_DDRV:
		module = ddr_module_desc;
		tbl_size = sizeof(ddr_module_desc) / sizeof(char *);
		break;

	case JM_SUB_SYS_GIC:
		module = gic_module_desc;
		tbl_size = sizeof(gic_module_desc) / sizeof(char *);
		break;
	case JM_SUB_SYS_IOSUB:
		module = iosub_module_desc;
		tbl_size = sizeof(iosub_module_desc) / sizeof(char *);
		break;
	case JM_SUB_SYS_SCP:
		module = scp_module_desc;
		tbl_size = sizeof(scp_module_desc) / sizeof(char *);
		break;
	case JM_SUB_SYS_MCP:
		module = mcp_module_desc;
		tbl_size = sizeof(mcp_module_desc) / sizeof(char *);
		break;
	case JM_SUB_SYS_IMU0:
	case JM_SUB_SYS_IMU1:
		module = imu_module_desc;
		tbl_size = sizeof(imu_module_desc) / sizeof(char *);
		break;
	case JM_SUB_SYS_DPE:
		module = dpe_module_desc;
		tbl_size = sizeof(dpe_module_desc) / sizeof(char *);
		break;
	case JM_SUB_SYS_RPE:
		module = rpe_module_desc;
		tbl_size = sizeof(rpe_module_desc) / sizeof(char *);
		break;
	case JM_SUB_SYS_PSUB:
		module = psub_module_desc;
		tbl_size = sizeof(psub_module_desc) / sizeof(char *);
		break;
	case JM_SUB_SYS_HAC:
		module = hac_module_desc;
		tbl_size = sizeof(hac_module_desc) / sizeof(char *);
		break;
	case JM_SUB_SYS_TCM:
		module = tcm_module_desc;
		tbl_size = sizeof(tcm_module_desc) / sizeof(char *);
		break;

	default:
		module = NULL;
		break;
	}

	if ((!module) || (mod_id >= tbl_size))
		return "unknown";

	return module[mod_id];
}

static const char *get_jm_submod_desc(uint8_t subsys_id, uint8_t mod_id, uint8_t sub_id)
{
	const char * const*sub_module;
	int tbl_size;

	if (subsys_id == JM_SUB_SYS_IOSUB && mod_id == JM_SUBSYS_IOSUB_MOD_SMMU) {
		sub_module = iosub_smmu_sub_desc;
		tbl_size = sizeof(iosub_smmu_sub_desc) / sizeof(char *);
	} else if (subsys_id == JM_SUB_SYS_IOSUB && mod_id == JM_SUBSYS_IOSUB_MOD_OTHER) {
		sub_module = iosub_other_sub_desc;
		tbl_size = sizeof(iosub_other_sub_desc) / sizeof(char *);
	} else if (subsys_id == JM_SUB_SYS_DPE && mod_id == JM_SUBSYS_DPE_MOD_SMMU) {
		sub_module = smmu_sub_desc;
		tbl_size = sizeof(smmu_sub_desc) / sizeof(char *);
	} else if (subsys_id == JM_SUB_SYS_RPE && mod_id == JM_SUBSYS_RPE_MOD_SMMU) {
		sub_module = smmu_sub_desc;
		tbl_size = sizeof(smmu_sub_desc) / sizeof(char *);
	} else if (subsys_id == JM_SUB_SYS_PSUB && mod_id == JM_SUBSYS_PSUB_MOD_PCIE0) {
		sub_module = psub_pcie0_sub_desc;
		tbl_size = sizeof(psub_pcie0_sub_desc) / sizeof(char *);
	} else if (subsys_id == JM_SUB_SYS_PSUB && mod_id == JM_SUBSYS_PSUB_MOD_X2RC_SMMU) {
		sub_module = smmu_sub_desc;
		tbl_size = sizeof(smmu_sub_desc) / sizeof(char *);
	} else if (subsys_id == JM_SUB_SYS_PSUB && mod_id == JM_SUBSYS_PSUB_MOD_X16RC_SMMU) {
		sub_module = smmu_sub_desc;
		tbl_size = sizeof(smmu_sub_desc) / sizeof(char *);
	} else if (subsys_id == JM_SUB_SYS_PSUB && mod_id == JM_SUBSYS_PSUB_MOD_SDMA_SMMU) {
		sub_module = smmu_sub_desc;
		tbl_size = sizeof(smmu_sub_desc) / sizeof(char *);
	} else if (subsys_id == JM_SUB_SYS_TCM && mod_id == JM_SUBSYS_TCM_MOD_SMMU) {
		sub_module = smmu_sub_desc;
		tbl_size = sizeof(smmu_sub_desc) / sizeof(char *);
	} else {
		sub_module = NULL;
		tbl_size = 0;
	}

	if ((!sub_module) || (sub_id >= tbl_size))
		return "unknown";

	return sub_module[sub_id];
}

static const char *get_jm_dev_desc(uint8_t subsys_id, uint8_t mod_id, uint8_t sub_id)
{
	if (subsys_id == JM_SUB_SYS_CSUB)
		return csub_dev_desc[0];
	else if (subsys_id == JM_SUB_SYS_DDRH || subsys_id == JM_SUB_SYS_DDRV)
		return ddr_dev_desc[0];
	else if (subsys_id == JM_SUB_SYS_CMN)
		return cmn_dev_desc[0];
	else
		return default_dev_desc[0];
}

#define JM_ERR_SEVERITY_NFE		0
#define JM_ERR_SEVERITY_FE		1
#define JM_ERR_SEVERITY_CE		2
#define JM_ERR_SEVERITY_NONE	3

/* helper functions */
static inline char *jm_err_severity(uint8_t err_sev)
{
	switch (err_sev) {
	case JM_ERR_SEVERITY_NFE: return "recoverable";
	case JM_ERR_SEVERITY_FE: return "fatal";
	case JM_ERR_SEVERITY_CE: return "corrected";
	case JM_ERR_SEVERITY_NONE: return "none";
	default:
		break;
	}
	return "unknown";
}

static void decode_jm_common_sec_head(struct ras_ns_ev_decoder *ev_decoder,
				      const struct jm_common_sec_head *err,
					struct jm_event *event)
{
	if (err->val_bits & BIT(JM_COMMON_VALID_SOC_ID)) {
		JM_SNPRINTF(event->error_msg, "[ table_version=%hhu decode_version:%hhu",
			    err->version, PAYLOAD_VERSION);
		record_jm_data(ev_decoder, JM_OEM_DATA_TYPE_INT,
			       JM_PAYLOAD_FIELD_VERSION,
					err->version, NULL);
	}

	if (err->val_bits & BIT(JM_COMMON_VALID_SOC_ID)) {
		JM_SNPRINTF(event->error_msg, " soc=%s", get_jm_soc_desc(err->soc_id));
		record_jm_data(ev_decoder, JM_OEM_DATA_TYPE_INT,
			       JM_PAYLOAD_FIELD_SOC_ID,
				err->soc_id, NULL);
	}

	if (err->val_bits & BIT(JM_COMMON_VALID_SUBSYSTEM_ID)) {
		JM_SNPRINTF(event->error_msg, " sub system=%s",
			    get_jm_subsystem_desc(err->subsystem_id));
		record_jm_data(ev_decoder, JM_OEM_DATA_TYPE_TEXT,
			       JM_PAYLOAD_FIELD_SUB_SYS,
					0, get_jm_subsystem_desc(err->subsystem_id));
	}

	if (err->val_bits & BIT(JM_COMMON_VALID_MODULE_ID)) {
		JM_SNPRINTF(event->error_msg, " module=%s",
			    get_jm_module_desc(err->subsystem_id, err->module_id));
		record_jm_data(ev_decoder, JM_OEM_DATA_TYPE_TEXT,
			       JM_PAYLOAD_FIELD_MODULE,
				0, get_jm_module_desc(err->subsystem_id, err->module_id));
		record_jm_data(ev_decoder, JM_OEM_DATA_TYPE_INT,
			       JM_PAYLOAD_FIELD_MODULE_ID,
				err->module_id, NULL);
	}

	if (err->val_bits & BIT(JM_COMMON_VALID_SUBMODULE_ID)) {
		JM_SNPRINTF(event->error_msg, " sub module=%s",
			    get_jm_submod_desc(err->subsystem_id, err->module_id, err->submodule_id));
		record_jm_data(ev_decoder, JM_OEM_DATA_TYPE_TEXT,
			       JM_PAYLOAD_FIELD_SUB_MODULE,
			0,
			get_jm_submod_desc(err->subsystem_id, err->module_id, err->submodule_id));
		record_jm_data(ev_decoder, JM_OEM_DATA_TYPE_INT,
			       JM_PAYLOAD_FIELD_MODULE_ID,
			err->submodule_id, NULL);
	}

	if (err->val_bits & BIT(JM_COMMON_VALID_DEV_ID)) {
		JM_SNPRINTF(event->error_msg, " dev=%s",
			    get_jm_dev_desc(err->subsystem_id, err->module_id, err->submodule_id));
		record_jm_data(ev_decoder, JM_OEM_DATA_TYPE_TEXT,
			       JM_PAYLOAD_FIELD_DEV,
			0, get_jm_dev_desc(err->subsystem_id, err->module_id, err->submodule_id));
		record_jm_data(ev_decoder, JM_OEM_DATA_TYPE_INT,
			       JM_PAYLOAD_FIELD_DEV_ID,
					err->dev_id, NULL);
	}

	if (err->val_bits & BIT(JM_COMMON_VALID_ERR_TYPE)) {
		JM_SNPRINTF(event->error_msg, " err_type=%hu", err->err_type);
		record_jm_data(ev_decoder, JM_OEM_DATA_TYPE_INT,
			       JM_PAYLOAD_FIELD_ERR_TYPE,
					err->err_type, NULL);
	}

	if (err->val_bits & BIT(JM_COMMON_VALID_ERR_SEVERITY)) {
		JM_SNPRINTF(event->error_msg, " err_severity=%s",
			    jm_err_severity(err->err_severity));
		record_jm_data(ev_decoder, JM_OEM_DATA_TYPE_TEXT,
			       JM_PAYLOAD_FIELD_ERR_SEVERITY,
					0, jm_err_severity(err->err_severity));
	}

	JM_SNPRINTF(event->error_msg, "]");
}

static void decode_jm_common_sec_tail(struct ras_ns_ev_decoder *ev_decoder,
				      const struct jm_common_sec_tail *err,
					struct jm_event *event, uint32_t val_bits)
{
	if (val_bits & BIT(JM_COMMON_VALID_REG_ARRAY_SIZE) && err->reg_array_size > 0) {
		int i;

		JM_SNPRINTF(event->reg_msg, "Extended Register Dump:");
		for (i = 0; i < err->reg_array_size; i++) {
			JM_SNPRINTF(event->reg_msg, "reg%02d=0x%08x",
				    i, err->reg_array[i]);
		}
	}
}

#ifdef HAVE_SQLITE3

/*key pair definition for jaguar micro specific error payload type 0*/
static const struct db_fields jm_payload0_event_fields[] = {
	{ .name = "id",					.type = "INTEGER PRIMARY KEY" },
	{ .name = "timestamp",			.type = "TEXT" },
	{ .name = "version",			.type = "INTEGER" },
	{ .name = "soc_id",				.type = "INTEGER" },
	{ .name = "subsystem",			.type = "TEXT" },
	{ .name = "module",				.type = "TEXT" },
	{ .name = "module_id",			.type = "INTEGER" },
	{ .name = "sub_module",			.type = "TEXT" },
	{ .name = "submodule_id",		.type = "INTEGER" },
	{ .name = "dev",				.type = "TEXT" },
	{ .name = "dev_id",				.type = "INTEGER" },
	{ .name = "err_type",			.type = "INTEGER" },
	{ .name = "err_severity",		.type = "TEXT" },
	{ .name = "regs_dump",			.type = "TEXT" },
};

static const struct db_table_descriptor jm_payload0_event_tab = {
	.name = "jm_payload0_event",
	.fields = jm_payload0_event_fields,
	.num_fields = ARRAY_SIZE(jm_payload0_event_fields),
};

/*Save data with different type into sqlite3 db*/
static void record_jm_data(struct ras_ns_ev_decoder *ev_decoder,
			   enum jm_oem_data_type data_type, int id,
				int64_t data, const char *text)
{
	switch (data_type) {
	case JM_OEM_DATA_TYPE_INT:
		sqlite3_bind_int(ev_decoder->stmt_dec_record, id, data);
		break;
	case JM_OEM_DATA_TYPE_INT64:
		sqlite3_bind_int64(ev_decoder->stmt_dec_record, id, data);
		break;
	case JM_OEM_DATA_TYPE_TEXT:
		sqlite3_bind_text(ev_decoder->stmt_dec_record, id, text,
				  -1, NULL);
		break;
	default:
		break;
	}
}

static int store_jm_err_data(struct ras_ns_ev_decoder *ev_decoder,
			     const char *tab_name)
{
	int rc;

	rc = sqlite3_step(ev_decoder->stmt_dec_record);
	if (rc != SQLITE_OK && rc != SQLITE_DONE)
		log(TERM, LOG_ERR,
		    "Failed to do step on sqlite. Table = %s error = %d\n",
			tab_name, rc);

	rc = sqlite3_reset(ev_decoder->stmt_dec_record);
	if (rc != SQLITE_OK && rc != SQLITE_DONE)
		log(TERM, LOG_ERR,
		    "Failed to reset on sqlite. Table = %s error = %d\n",
			tab_name, rc);

	rc = sqlite3_clear_bindings(ev_decoder->stmt_dec_record);
	if (rc != SQLITE_OK && rc != SQLITE_DONE)
		log(TERM, LOG_ERR,
		    "Failed to clear bindings on sqlite. Table = %s error = %d\n",
			tab_name, rc);

	return rc;
}

/*save all JaguarMicro Specific Error Payload type 0 to sqlite3 database*/
static void record_jm_payload_err(struct ras_ns_ev_decoder *ev_decoder,
				  const char *reg_str)
{
	if (ev_decoder) {
		record_jm_data(ev_decoder, JM_OEM_DATA_TYPE_TEXT,
			       JM_PAYLOAD_FIELD_REGS_DUMP, 0, reg_str);
		store_jm_err_data(ev_decoder, "jm_payload0_event_tab");
	}
}

#else
static void record_jm_data(struct ras_ns_ev_decoder *ev_decoder,
			   enum jm_oem_data_type data_type,
				int id, int64_t data, const char *text)
{
}

static void record_jm_payload_err(struct ras_ns_ev_decoder *ev_decoder,
				  const char *reg_str)
{
}

#endif

/*decode JaguarMicro specific error payload type 0, the CPU's data is save*/
/*to sqlite by ras-arm-handler, others are saved by this function.*/
static void decode_jm_payload0_err_regs(struct ras_ns_ev_decoder *ev_decoder,
					struct trace_seq *s,
				const struct jm_payload0_type_sec *err)
{
	int i = 0;

	const struct jm_common_sec_head *common_head = &err->common_head;
	const struct jm_common_sec_tail *common_tail = &err->common_tail;

	struct jm_event jmevent;

	memset(&jmevent, 0, sizeof(struct jm_event));
	trace_seq_printf(s, "\nJaguar Micro Common Error Section:\n");
	decode_jm_common_sec_head(ev_decoder, common_head, &jmevent);
	trace_seq_printf(s, "%s\n", jmevent.error_msg);

	//display lock_control
	JM_SNPRINTF(jmevent.reg_msg, " %s", disp_payload0_err_reg_name[i++]);
	JM_SNPRINTF(jmevent.reg_msg, " 0x%x; ", err->lock_control);

	//display lock_function
	JM_SNPRINTF(jmevent.reg_msg, " %s", disp_payload0_err_reg_name[i++]);
	JM_SNPRINTF(jmevent.reg_msg, " 0x%x; ", err->lock_function);

	//display cfg_ram_id
	JM_SNPRINTF(jmevent.reg_msg, " %s", disp_payload0_err_reg_name[i++]);
	JM_SNPRINTF(jmevent.reg_msg, " 0x%x; ", err->cfg_ram_id);

	//display err_fr_low32
	JM_SNPRINTF(jmevent.reg_msg, " %s", disp_payload0_err_reg_name[i++]);
	JM_SNPRINTF(jmevent.reg_msg, " 0x%x; ", err->err_fr_low32);

	//display err_fr_high32
	JM_SNPRINTF(jmevent.reg_msg, " %s", disp_payload0_err_reg_name[i++]);
	JM_SNPRINTF(jmevent.reg_msg, " 0x%x; ", err->err_fr_high32);

	//display err_ctlr_low32
	JM_SNPRINTF(jmevent.reg_msg, " %s", disp_payload0_err_reg_name[i++]);
	JM_SNPRINTF(jmevent.reg_msg, " 0x%x; ", err->err_ctlr_low32);

	//display ecc_status_low32
	JM_SNPRINTF(jmevent.reg_msg, " %s", disp_payload0_err_reg_name[i++]);
	JM_SNPRINTF(jmevent.reg_msg, " 0x%x; ", err->ecc_status_low32);

	//display ecc_addr_low32
	JM_SNPRINTF(jmevent.reg_msg, " %s", disp_payload0_err_reg_name[i++]);
	JM_SNPRINTF(jmevent.reg_msg, " 0x%x; ", err->ecc_addr_low32);

	//display ecc_addr_high32
	JM_SNPRINTF(jmevent.reg_msg, " %s", disp_payload0_err_reg_name[i++]);
	JM_SNPRINTF(jmevent.reg_msg, " 0x%x; ", err->ecc_addr_high32);

	//display ecc_misc0_low32
	JM_SNPRINTF(jmevent.reg_msg, " %s", disp_payload0_err_reg_name[i++]);
	JM_SNPRINTF(jmevent.reg_msg, " 0x%x; ", err->ecc_misc0_low32);

	//display ecc_misc0_high32
	JM_SNPRINTF(jmevent.reg_msg, " %s", disp_payload0_err_reg_name[i++]);
	JM_SNPRINTF(jmevent.reg_msg, " 0x%x; ", err->ecc_misc0_high32);

	//display ecc_misc1_low32
	JM_SNPRINTF(jmevent.reg_msg, " %s", disp_payload0_err_reg_name[i++]);
	JM_SNPRINTF(jmevent.reg_msg, " 0x%x; ", err->ecc_misc1_low32);

	//display ecc_misc1_high32
	JM_SNPRINTF(jmevent.reg_msg, " %s", disp_payload0_err_reg_name[i++]);
	JM_SNPRINTF(jmevent.reg_msg, " 0x%x; ", err->ecc_misc1_high32);

	//display ecc_misc2_Low32
	JM_SNPRINTF(jmevent.reg_msg, " %s", disp_payload0_err_reg_name[i++]);
	JM_SNPRINTF(jmevent.reg_msg, " 0x%x; ", err->ecc_misc2_Low32);

	//display ecc_misc2_high32
	JM_SNPRINTF(jmevent.reg_msg, " %s", disp_payload0_err_reg_name[i++]);
	JM_SNPRINTF(jmevent.reg_msg, " 0x%x\n", err->ecc_misc2_high32);

	trace_seq_printf(s, "Register Dump:\n");
	decode_jm_common_sec_tail(ev_decoder, common_tail, &jmevent, common_head->val_bits);

	record_jm_payload_err(ev_decoder, jmevent.reg_msg);

	trace_seq_printf(s, "%s\n", jmevent.reg_msg);
}

/*decode JaguarMicro specific error payload type 1 and save to sqlite db*/
static void decode_jm_payload1_err_regs(struct ras_ns_ev_decoder *ev_decoder,
					struct trace_seq *s,
					const struct jm_payload1_type_sec *err)
{
	int i = 0;

	const struct jm_common_sec_head *common_head = &err->common_head;
	const struct jm_common_sec_tail *common_tail = &err->common_tail;

	struct jm_event jmevent;

	memset(&jmevent, 0, sizeof(struct jm_event));
	trace_seq_printf(s, "\nJaguarMicro Common Error Section:\n");
	decode_jm_common_sec_head(ev_decoder, common_head, &jmevent);
	trace_seq_printf(s, "%s\n", jmevent.error_msg);

	//display smmu csr(Inturrpt status)
	JM_SNPRINTF(jmevent.reg_msg, " %s", disp_payload1_err_reg_name[i++]);
	JM_SNPRINTF(jmevent.reg_msg, " 0x%x; ", err->smmu_csr);

	//display ERRFR
	JM_SNPRINTF(jmevent.reg_msg, " %s", disp_payload1_err_reg_name[i++]);
	JM_SNPRINTF(jmevent.reg_msg, " 0x%x; ", err->errfr);

	//display ERRCTLR
	JM_SNPRINTF(jmevent.reg_msg, " %s", disp_payload1_err_reg_name[i++]);
	JM_SNPRINTF(jmevent.reg_msg, " 0x%x; ", err->errctlr);

	//display ERRSTATUS
	JM_SNPRINTF(jmevent.reg_msg, " %s", disp_payload1_err_reg_name[i++]);
	JM_SNPRINTF(jmevent.reg_msg, " 0x%x; ", err->errstatus);

	//display ERRGEN
	JM_SNPRINTF(jmevent.reg_msg, " %s", disp_payload1_err_reg_name[i++]);
	JM_SNPRINTF(jmevent.reg_msg, " 0x%x\n", err->errgen);

	trace_seq_printf(s, "Register Dump:\n");
	decode_jm_common_sec_tail(ev_decoder, common_tail, &jmevent, common_head->val_bits);

	record_jm_payload_err(ev_decoder, jmevent.reg_msg);

	trace_seq_printf(s, "%s\n", jmevent.reg_msg);
}

/*decode JaguarMicro specific error payload type 2 and save to sqlite db*/
static void decode_jm_payload2_err_regs(struct ras_ns_ev_decoder *ev_decoder,
					struct trace_seq *s,
					const struct jm_payload2_type_sec *err)
{
	int i = 0;

	const struct jm_common_sec_head *common_head = &err->common_head;
	const struct jm_common_sec_tail *common_tail = &err->common_tail;

	struct jm_event jmevent;

	memset(&jmevent, 0, sizeof(struct jm_event));
	trace_seq_printf(s, "\nJaguarMicro Common Error Section:\n");
	decode_jm_common_sec_head(ev_decoder, common_head, &jmevent);
	trace_seq_printf(s, "%s\n", jmevent.error_msg);

	//display ecc_1bit_error_interrupt_low
	JM_SNPRINTF(jmevent.reg_msg,  " %s", disp_payload2_err_reg_name[i++]);
	JM_SNPRINTF(jmevent.reg_msg,  " 0x%x; ", err->ecc_1bit_int_low);

	//display ecc_1bit_error_interrupt_high
	JM_SNPRINTF(jmevent.reg_msg,  " %s", disp_payload2_err_reg_name[i++]);
	JM_SNPRINTF(jmevent.reg_msg,  " 0x%x; ", err->ecc_1bit_int_high);

	//display ecc_2bit_error_interrupt_low
	JM_SNPRINTF(jmevent.reg_msg,  " %s", disp_payload2_err_reg_name[i++]);
	JM_SNPRINTF(jmevent.reg_msg,  " 0x%x; ", err->ecc_2bit_int_low);

	//display ecc_2bit_error_interrupt_high
	JM_SNPRINTF(jmevent.reg_msg,  " %s", disp_payload2_err_reg_name[i++]);
	JM_SNPRINTF(jmevent.reg_msg,  " 0x%x\n", err->ecc_2bit_int_high);

	trace_seq_printf(s, "Register Dump:\n");
	decode_jm_common_sec_tail(ev_decoder, common_tail, &jmevent, common_head->val_bits);

	record_jm_payload_err(ev_decoder, jmevent.reg_msg);

	trace_seq_printf(s, "%s\n", jmevent.reg_msg);
}

/*decode JaguarMicro specific error payload type 5 and save to sqlite db*/
static void decode_jm_payload5_err_regs(struct ras_ns_ev_decoder *ev_decoder,
					struct trace_seq *s,
					const struct jm_payload5_type_sec *err)
{
	int i = 0;

	const struct jm_common_sec_head *common_head = &err->common_head;
	const struct jm_common_sec_tail *common_tail = &err->common_tail;

	struct jm_event jmevent;

	memset(&jmevent, 0, sizeof(struct jm_event));
	trace_seq_printf(s, "\nJaguarMicro Common Error Section:\n");
	decode_jm_common_sec_head(ev_decoder, common_head, &jmevent);
	trace_seq_printf(s, "%s\n", jmevent.error_msg);

	//display cfgm_mxp_0
	JM_SNPRINTF(jmevent.reg_msg, " %s", disp_payload5_err_reg_name[i++]);
	JM_SNPRINTF(jmevent.reg_msg, " 0x%llx; ", (unsigned long long)err->cfgm_mxp_0);

	//display cfgm_hnf_0
	JM_SNPRINTF(jmevent.reg_msg, " %s", disp_payload5_err_reg_name[i++]);
	JM_SNPRINTF(jmevent.reg_msg, " 0x%llx; ", (unsigned long long)err->cfgm_hnf_0);

	//display cfgm_hni_0
	JM_SNPRINTF(jmevent.reg_msg, " %s", disp_payload5_err_reg_name[i++]);
	JM_SNPRINTF(jmevent.reg_msg, " 0x%llx; ", (unsigned long long)err->cfgm_hni_0);

	//display cfgm_sbsx_0
	JM_SNPRINTF(jmevent.reg_msg, " %s", disp_payload5_err_reg_name[i++]);
	JM_SNPRINTF(jmevent.reg_msg, " 0x%llx; ", (unsigned long long)err->cfgm_sbsx_0);

	//display errfr_NS
	JM_SNPRINTF(jmevent.reg_msg, " %s", disp_payload5_err_reg_name[i++]);
	JM_SNPRINTF(jmevent.reg_msg, " 0x%llx; ", (unsigned long long)err->errfr_NS);

	//display errctlrr_NS
	JM_SNPRINTF(jmevent.reg_msg, " %s", disp_payload5_err_reg_name[i++]);
	JM_SNPRINTF(jmevent.reg_msg, " 0x%llx; ", (unsigned long long)err->errctlrr_NS);

	//display errstatusr_NS
	JM_SNPRINTF(jmevent.reg_msg, " %s", disp_payload5_err_reg_name[i++]);
	JM_SNPRINTF(jmevent.reg_msg, " 0x%llx; ", (unsigned long long)err->errstatusr_NS);

	//display erraddrr_NS
	JM_SNPRINTF(jmevent.reg_msg, " %s", disp_payload5_err_reg_name[i++]);
	JM_SNPRINTF(jmevent.reg_msg, " 0x%llx; ", (unsigned long long)err->erraddrr_NS);

	//display errmiscr_NS
	JM_SNPRINTF(jmevent.reg_msg, " %s", disp_payload5_err_reg_name[i++]);
	JM_SNPRINTF(jmevent.reg_msg, " 0x%llx; ", (unsigned long long)err->errmiscr_NS);

	//display errfr
	JM_SNPRINTF(jmevent.reg_msg, " %s", disp_payload5_err_reg_name[i++]);
	JM_SNPRINTF(jmevent.reg_msg, " 0x%llx; ", (unsigned long long)err->errfr);

	//display errctlr
	JM_SNPRINTF(jmevent.reg_msg, " %s", disp_payload5_err_reg_name[i++]);
	JM_SNPRINTF(jmevent.reg_msg, " 0x%llx; ", (unsigned long long)err->errctlr);

	//display errstatus
	JM_SNPRINTF(jmevent.reg_msg, " %s", disp_payload5_err_reg_name[i++]);
	JM_SNPRINTF(jmevent.reg_msg, " 0x%llx; ", (unsigned long long)err->errstatus);

	//display erraddr
	JM_SNPRINTF(jmevent.reg_msg, " %s", disp_payload5_err_reg_name[i++]);
	JM_SNPRINTF(jmevent.reg_msg, " 0x%llx; ", (unsigned long long)err->erraddr);

	//display errmisc
	JM_SNPRINTF(jmevent.reg_msg, " %s", disp_payload5_err_reg_name[i++]);
	JM_SNPRINTF(jmevent.reg_msg, " 0x%llx\n", (unsigned long long)err->errmisc);

	trace_seq_printf(s, "Register Dump:\n");
	decode_jm_common_sec_tail(ev_decoder, common_tail, &jmevent, common_head->val_bits);

	record_jm_payload_err(ev_decoder, jmevent.reg_msg);

	trace_seq_printf(s, "%s\n", jmevent.reg_msg);
}

/*decode JaguarMicro specific error payload type 6 and save to sqlite db*/
static void decode_jm_payload6_err_regs(struct ras_ns_ev_decoder *ev_decoder,
					struct trace_seq *s,
					const struct jm_payload6_type_sec *err)
{
	int i = 0;

	const struct jm_common_sec_head *common_head = &err->common_head;
	const struct jm_common_sec_tail *common_tail = &err->common_tail;

	struct jm_event jmevent;

	memset(&jmevent, 0, sizeof(struct jm_event));
	trace_seq_printf(s, "\nJaguarMicro Common Error Section:\n");
	decode_jm_common_sec_head(ev_decoder, common_head, &jmevent);
	trace_seq_printf(s, "%s\n", jmevent.error_msg);
	//display RECORD_ID
	JM_SNPRINTF(jmevent.reg_msg, " %s", disp_payload6_err_reg_name[i++]);
	JM_SNPRINTF(jmevent.reg_msg, " 0x%llx; ", (unsigned long long)err->record_id);

	//display GICT_ERR_FR
	JM_SNPRINTF(jmevent.reg_msg, " %s", disp_payload6_err_reg_name[i++]);
	JM_SNPRINTF(jmevent.reg_msg, " 0x%llx; ", (unsigned long long)err->gict_err_fr);

	//display GICT_ERR_CTLR
	JM_SNPRINTF(jmevent.reg_msg, " %s", disp_payload6_err_reg_name[i++]);
	JM_SNPRINTF(jmevent.reg_msg, " 0x%llx; ", (unsigned long long)err->gict_err_ctlr);

	//display GICT_ERR_STATUS
	JM_SNPRINTF(jmevent.reg_msg, " %s", disp_payload6_err_reg_name[i++]);
	JM_SNPRINTF(jmevent.reg_msg, " 0x%llx; ", (unsigned long long)err->gict_err_status);

	//display GICT_ERR_ADDR
	JM_SNPRINTF(jmevent.reg_msg, " %s", disp_payload6_err_reg_name[i++]);
	JM_SNPRINTF(jmevent.reg_msg, " 0x%llx; ", (unsigned long long)err->gict_err_addr);

	//display GICT_ERR_MISC0
	JM_SNPRINTF(jmevent.reg_msg, " %s", disp_payload6_err_reg_name[i++]);
	JM_SNPRINTF(jmevent.reg_msg, " 0x%llx; ", (unsigned long long)err->gict_err_misc0);

	//display GICT_ERR_MISC1
	JM_SNPRINTF(jmevent.reg_msg, " %s", disp_payload6_err_reg_name[i++]);
	JM_SNPRINTF(jmevent.reg_msg, " 0x%llx; ", (unsigned long long)err->gict_err_misc1);

	//display GICT_ERRGSR
	JM_SNPRINTF(jmevent.reg_msg, " %s", disp_payload6_err_reg_name[i++]);
	JM_SNPRINTF(jmevent.reg_msg, " 0x%llx\n", (unsigned long long)err->gict_errgsr);

	trace_seq_printf(s, "Register Dump:\n");
	decode_jm_common_sec_tail(ev_decoder, common_tail, &jmevent, common_head->val_bits);

	record_jm_payload_err(ev_decoder, jmevent.reg_msg);
	trace_seq_printf(s, "%s\n", jmevent.reg_msg);
}

/* error data decoding functions */
static int decode_jm_oem_type_error(struct ras_events *ras,
				    struct ras_ns_ev_decoder *ev_decoder,
					struct trace_seq *s,
					struct ras_non_standard_event *event,
					int payload_type)
{
	int id = JM_PAYLOAD_FIELD_TIMESTAMP;

	record_jm_data(ev_decoder, JM_OEM_DATA_TYPE_TEXT,
		       id, 0, event->timestamp);

	if (payload_type == PAYLOAD_TYPE_0) {
		const struct jm_payload0_type_sec *err =
			(struct jm_payload0_type_sec *)event->error;
		decode_jm_payload0_err_regs(ev_decoder, s, err);
	} else if (payload_type == PAYLOAD_TYPE_1) {
		const struct jm_payload1_type_sec *err =
			(struct jm_payload1_type_sec *)event->error;
		decode_jm_payload1_err_regs(ev_decoder, s, err);
	} else if (payload_type == PAYLOAD_TYPE_2) {
		const struct jm_payload2_type_sec *err =
			(struct jm_payload2_type_sec *)event->error;
		decode_jm_payload2_err_regs(ev_decoder, s, err);
	} else if (payload_type == PAYLOAD_TYPE_5) {
		const struct jm_payload5_type_sec *err =
			(struct jm_payload5_type_sec *)event->error;
		decode_jm_payload5_err_regs(ev_decoder, s, err);
	} else if (payload_type == PAYLOAD_TYPE_6) {
		const struct jm_payload6_type_sec *err =
			(struct jm_payload6_type_sec *)event->error;
		decode_jm_payload6_err_regs(ev_decoder, s, err);
	} else {
		trace_seq_printf(s, "%s : wrong payload type\n", __func__);
		log(TERM, LOG_ERR, "%s : wrong payload type\n", __func__);
		return -1;
	}
	return 0;
}

/* error type0 data decoding functions */
static int decode_jm_oem_type0_error(struct ras_events *ras,
				     struct ras_ns_ev_decoder *ev_decoder,
					struct trace_seq *s,
					struct ras_non_standard_event *event)
{
	return decode_jm_oem_type_error(ras, ev_decoder, s, event, PAYLOAD_TYPE_0);
}

/* error type1 data decoding functions */
static int decode_jm_oem_type1_error(struct ras_events *ras,
				     struct ras_ns_ev_decoder *ev_decoder,
					struct trace_seq *s,
					struct ras_non_standard_event *event)
{
	return decode_jm_oem_type_error(ras, ev_decoder, s, event, PAYLOAD_TYPE_1);
}

/* error type2 data decoding functions */
static int decode_jm_oem_type2_error(struct ras_events *ras,
				     struct ras_ns_ev_decoder *ev_decoder,
					struct trace_seq *s,
					struct ras_non_standard_event *event)
{
	return decode_jm_oem_type_error(ras, ev_decoder, s, event, PAYLOAD_TYPE_2);
}

/* error type5 data decoding functions */
static int decode_jm_oem_type5_error(struct ras_events *ras,
				     struct ras_ns_ev_decoder *ev_decoder,
					struct trace_seq *s,
					struct ras_non_standard_event *event)
{
	return decode_jm_oem_type_error(ras, ev_decoder, s, event, PAYLOAD_TYPE_5);
}

/* error type6 data decoding functions */
static int decode_jm_oem_type6_error(struct ras_events *ras,
				     struct ras_ns_ev_decoder *ev_decoder,
					struct trace_seq *s,
					struct ras_non_standard_event *event)
{
	return decode_jm_oem_type_error(ras, ev_decoder, s, event, PAYLOAD_TYPE_6);
}

static int add_jm_oem_type0_table(struct ras_events *ras, struct ras_ns_ev_decoder *ev_decoder)
{
#ifdef HAVE_SQLITE3
	if (ras->record_events && !ev_decoder->stmt_dec_record) {
		if (ras_mc_add_vendor_table(ras, &ev_decoder->stmt_dec_record,
					    &jm_payload0_event_tab) != SQLITE_OK) {
			log(TERM, LOG_WARNING, "Failed to create sql jm_payload0_event_tab\n");
			return -1;
		}
	}
#endif
	return 0;
}

struct ras_ns_ev_decoder jm_ns_oem_type_decoder[] = {
	{
		.sec_type = "82d78ba3-fa14-407a-ba0e-f3ba8170013c",
		.add_table = add_jm_oem_type0_table,
		.decode = decode_jm_oem_type0_error,
	},
	{
		.sec_type = "f9723053-2558-49b1-b58a-1c1a82492a62",
		.add_table = add_jm_oem_type0_table,
		.decode = decode_jm_oem_type1_error,
	},
	{
		.sec_type = "2d31de54-3037-4f24-a283-f69ca1ec0b9a",
		.add_table = add_jm_oem_type0_table,
		.decode = decode_jm_oem_type2_error,
	},
	{
		.sec_type = "dac80d69-0a72-4eba-8114-148ee344af06",
		.add_table = add_jm_oem_type0_table,
		.decode = decode_jm_oem_type5_error,
	},
	{
		.sec_type = "746f06fe-405e-451f-8d09-02e802ed984a",
		.add_table = add_jm_oem_type0_table,
		.decode = decode_jm_oem_type6_error,
	},
};

static void __attribute__((constructor)) jm_init(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(jm_ns_oem_type_decoder); i++)
		register_ns_ev_decoder(&jm_ns_oem_type_decoder[i]);
}

