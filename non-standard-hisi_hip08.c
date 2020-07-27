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
#include "non-standard-hisilicon.h"

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
#define HISI_OEM_MODULE_ID_L3T	5
#define HISI_OEM_MODULE_ID_L3D	6

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

#define HISI_PCIE_LOCAL_ERR_MISC_MAX	33
#define HISI_BUF_LEN	1024

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
	uint32_t   err_misc[HISI_PCIE_LOCAL_ERR_MISC_MAX];
};

enum {
	HIP08_OEM_TYPE1_FIELD_ID,
	HIP08_OEM_TYPE1_FIELD_TIMESTAMP,
	HIP08_OEM_TYPE1_FIELD_VERSION,
	HIP08_OEM_TYPE1_FIELD_SOC_ID,
	HIP08_OEM_TYPE1_FIELD_SOCKET_ID,
	HIP08_OEM_TYPE1_FIELD_NIMBUS_ID,
	HIP08_OEM_TYPE1_FIELD_MODULE_ID,
	HIP08_OEM_TYPE1_FIELD_SUB_MODULE_ID,
	HIP08_OEM_TYPE1_FIELD_ERR_SEV,
	HIP08_OEM_TYPE1_FIELD_REGS_DUMP,
};

enum {
	HIP08_OEM_TYPE2_FIELD_ID,
	HIP08_OEM_TYPE2_FIELD_TIMESTAMP,
	HIP08_OEM_TYPE2_FIELD_VERSION,
	HIP08_OEM_TYPE2_FIELD_SOC_ID,
	HIP08_OEM_TYPE2_FIELD_SOCKET_ID,
	HIP08_OEM_TYPE2_FIELD_NIMBUS_ID,
	HIP08_OEM_TYPE2_FIELD_MODULE_ID,
	HIP08_OEM_TYPE2_FIELD_SUB_MODULE_ID,
	HIP08_OEM_TYPE2_FIELD_ERR_SEV,
	HIP08_OEM_TYPE2_FIELD_REGS_DUMP,
};

enum {
	HIP08_PCIE_LOCAL_FIELD_ID,
	HIP08_PCIE_LOCAL_FIELD_TIMESTAMP,
	HIP08_PCIE_LOCAL_FIELD_VERSION,
	HIP08_PCIE_LOCAL_FIELD_SOC_ID,
	HIP08_PCIE_LOCAL_FIELD_SOCKET_ID,
	HIP08_PCIE_LOCAL_FIELD_NIMBUS_ID,
	HIP08_PCIE_LOCAL_FIELD_SUB_MODULE_ID,
	HIP08_PCIE_LOCAL_FIELD_CORE_ID,
	HIP08_PCIE_LOCAL_FIELD_PORT_ID,
	HIP08_PCIE_LOCAL_FIELD_ERR_SEV,
	HIP08_PCIE_LOCAL_FIELD_ERR_TYPE,
	HIP08_PCIE_LOCAL_FIELD_REGS_DUMP,
};

struct hisi_module_info {
	int id;
	const char *name;
	const char **sub;
	int sub_num;
};

static const char *pll_submodule_name[] = {
	"TB_PLL0",
	"TB_PLL1",
	"TB_PLL2",
	"TB_PLL3",
	"TA_PLL0",
	"TA_PLL1",
	"TA_PLL2",
	"TA_PLL3",
	"NIMBUS_PLL0",
	"NIMBUS_PLL1",
	"NIMBUS_PLL2",
	"NIMBUS_PLL3",
	"NIMBUS_PLL4",
};

static const char *sllc_submodule_name[] = {
	"TB_SLLC0",
	"TB_SLLC1",
	"TB_SLLC2",
	"TA_SLLC0",
	"TA_SLLC1",
	"TA_SLLC2",
	"NIMBUS_SLLC0",
	"NIMBUS_SLLC1",
};

static const char *sioe_submodule_name[] = {
	"TB_SIOE0",
	"TB_SIOE1",
	"TB_SIOE2",
	"TB_SIOE3",
	"TA_SIOE0",
	"TA_SIOE1",
	"TA_SIOE2",
	"TA_SIOE3",
	"NIMBUS_SIOE0",
	"NIMBUS_SIOE1",
};

static const char *poe_submodule_name[] = {
	"TB_POE",
	"TA_POE",
};

static const char *disp_submodule_name[] = {
	"TB_PERI_DISP",
	"TB_POE_DISP",
	"TB_GIC_DISP",
	"TA_PERI_DISP",
	"TA_POE_DISP",
	"TA_GIC_DISP",
	"HAC_DISP",
	"PCIE_DISP",
	"IO_MGMT_DISP",
	"NETWORK_DISP",
};

static const char *sas_submodule_name[] = {
	"SAS0",
	"SAS1",
};

static const struct hisi_module_info hisi_oem_type1_module[] = {
	{
		.id = HISI_OEM_MODULE_ID_PLL,
		.name = "PLL",
		.sub = pll_submodule_name,
		.sub_num = ARRAY_SIZE(pll_submodule_name),
	},
	{
		.id = HISI_OEM_MODULE_ID_SAS,
		.name = "SAS",
		.sub = sas_submodule_name,
		.sub_num = ARRAY_SIZE(sas_submodule_name),
	},
	{
		.id = HISI_OEM_MODULE_ID_POE,
		.name = "POE",
		.sub = poe_submodule_name,
		.sub_num = ARRAY_SIZE(poe_submodule_name),
	},
	{
		.id = HISI_OEM_MODULE_ID_SLLC,
		.name = "SLLC",
		.sub = sllc_submodule_name,
		.sub_num = ARRAY_SIZE(sllc_submodule_name),
	},
	{
		.id = HISI_OEM_MODULE_ID_SIOE,
		.name = "SIOE",
		.sub = sioe_submodule_name,
		.sub_num = ARRAY_SIZE(sioe_submodule_name),
	},
	{
		.id = HISI_OEM_MODULE_ID_DISP,
		.name = "DISP",
		.sub = disp_submodule_name,
		.sub_num = ARRAY_SIZE(disp_submodule_name),
	},
	{
		.id = HISI_OEM_MODULE_ID_MN,
		.name = "MN",
	},
	{
		.id = HISI_OEM_MODULE_ID_AA,
		.name = "AA",
	},
	{
		.id = HISI_OEM_MODULE_ID_LPC,
		.name = "LPC",
	},
	{
		.id = HISI_OEM_MODULE_ID_GIC,
		.name = "GIC",
	},
	{
		.id = HISI_OEM_MODULE_ID_RDE,
		.name = "RDE",
	},
	{
		.id = HISI_OEM_MODULE_ID_SATA,
		.name = "SATA",
	},
	{
		.id = HISI_OEM_MODULE_ID_USB,
		.name = "USB",
	},
	{
	}
};

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
	"TB_HHA0",
	"TB_HHA1",
	"TA_HHA0",
	"TA_HHA1"
};

static const char *ddrc_submodule_name[] = {
	"TB_DDRC0",
	"TB_DDRC1",
	"TB_DDRC2",
	"TB_DDRC3",
	"TA_DDRC0",
	"TA_DDRC1",
	"TA_DDRC2",
	"TA_DDRC3",
};

static const char *l3tag_submodule_name[] = {
	"TB_PARTITION0",
	"TB_PARTITION1",
	"TB_PARTITION2",
	"TB_PARTITION3",
	"TB_PARTITION4",
	"TB_PARTITION5",
	"TB_PARTITION6",
	"TB_PARTITION7",
	"TA_PARTITION0",
	"TA_PARTITION1",
	"TA_PARTITION2",
	"TA_PARTITION3",
	"TA_PARTITION4",
	"TA_PARTITION5",
	"TA_PARTITION6",
	"TA_PARTITION7",
};

static const char *l3data_submodule_name[] = {
	"TB_BANK0",
	"TB_BANK1",
	"TB_BANK2",
	"TB_BANK3",
	"TA_BANK0",
	"TA_BANK1",
	"TA_BANK2",
	"TA_BANK3",
};

static const struct hisi_module_info hisi_oem_type2_module[] = {
	{
		.id = HISI_OEM_MODULE_ID_SMMU,
		.name = "SMMU",
		.sub = smmu_submodule_name,
		.sub_num = ARRAY_SIZE(smmu_submodule_name),
	},
	{
		.id = HISI_OEM_MODULE_ID_HHA,
		.name = "HHA",
		.sub = hha_submodule_name,
		.sub_num = ARRAY_SIZE(hha_submodule_name),
	},
	{
		.id = HISI_OEM_MODULE_ID_PA,
		.name = "PA",
	},
	{
		.id = HISI_OEM_MODULE_ID_HLLC,
		.name = "HLLC",
		.sub = hllc_submodule_name,
		.sub_num = ARRAY_SIZE(hllc_submodule_name),
	},
	{
		.id = HISI_OEM_MODULE_ID_DDRC,
		.name = "DDRC",
		.sub = ddrc_submodule_name,
		.sub_num = ARRAY_SIZE(ddrc_submodule_name),
	},
	{
		.id = HISI_OEM_MODULE_ID_L3T,
		.name = "L3TAG",
		.sub = l3tag_submodule_name,
		.sub_num = ARRAY_SIZE(l3tag_submodule_name),
	},
	{
		.id = HISI_OEM_MODULE_ID_L3D,
		.name = "L3DATA",
		.sub = l3data_submodule_name,
		.sub_num = ARRAY_SIZE(l3data_submodule_name),
	},
	{
	}
};

static const char *oem_module_name(const struct hisi_module_info *info,
				   uint8_t module_id)
{
	const struct hisi_module_info *module = &info[0];

	for (; module->name; module++) {
		if (module->id != module_id)
			continue;

		return module->name;
	}

	return "unknown";
}

static const char *oem_submodule_name(const struct hisi_module_info *info,
				      uint8_t module_id, uint8_t sub_module_id)
{
	const struct hisi_module_info *module = &info[0];

	for (; module->name; module++) {
		const char **submodule = module->sub;

		if (module->id != module_id)
			continue;

		if (module->sub == NULL)
			return module->name;

		if (sub_module_id >= module->sub_num)
			return "unknown";

		return submodule[sub_module_id];
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
	default:
		break;
	}
	return "unknown";
}

#ifdef HAVE_SQLITE3
static const struct db_fields hip08_oem_event_fields[] = {
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
	.fields = hip08_oem_event_fields,
	.num_fields = ARRAY_SIZE(hip08_oem_event_fields),
};

static const struct db_table_descriptor hip08_oem_type2_event_tab = {
	.name = "hip08_oem_type2_event_v2",
	.fields = hip08_oem_event_fields,
	.num_fields = ARRAY_SIZE(hip08_oem_event_fields),
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
#endif

#define IN_RANGE(p, start, end) ((p) >= (start) && (p) < (end))
static void decode_oem_type1_err_hdr(struct ras_ns_dec_tab *dec_tab,
				     struct trace_seq *s,
				     const struct hisi_oem_type1_err_sec *err)
{
	char buf[HISI_BUF_LEN];
	char *p = buf;
	char *end = buf + HISI_BUF_LEN;

	p += snprintf(p, end - p, "[ table_version=%d ", err->version);
	record_vendor_data(dec_tab, HISI_OEM_DATA_TYPE_INT,
			   HIP08_OEM_TYPE1_FIELD_VERSION, err->version, NULL);

	if (err->val_bits & HISI_OEM_VALID_SOC_ID && IN_RANGE(p, buf, end)) {
		p += snprintf(p, end - p, "SOC_ID=%d ", err->soc_id);
		record_vendor_data(dec_tab, HISI_OEM_DATA_TYPE_INT,
				   HIP08_OEM_TYPE1_FIELD_SOC_ID,
				   err->soc_id, NULL);
	}

	if (err->val_bits & HISI_OEM_VALID_SOCKET_ID && IN_RANGE(p, buf, end)) {
		p += snprintf(p, end - p, "socket_ID=%d ", err->socket_id);
		record_vendor_data(dec_tab, HISI_OEM_DATA_TYPE_INT,
				   HIP08_OEM_TYPE1_FIELD_SOCKET_ID,
				   err->socket_id, NULL);
	}

	if (err->val_bits & HISI_OEM_VALID_NIMBUS_ID && IN_RANGE(p, buf, end)) {
		p += snprintf(p, end - p, "nimbus_ID=%d ", err->nimbus_id);
		record_vendor_data(dec_tab, HISI_OEM_DATA_TYPE_INT,
				   HIP08_OEM_TYPE1_FIELD_NIMBUS_ID,
				   err->nimbus_id, NULL);
	}

	if (err->val_bits & HISI_OEM_VALID_MODULE_ID && IN_RANGE(p, buf, end)) {
		const char *str = oem_module_name(hisi_oem_type1_module,
						  err->module_id);

		p += snprintf(p, end - p, "module=%s ", str);
		record_vendor_data(dec_tab, HISI_OEM_DATA_TYPE_TEXT,
				   HIP08_OEM_TYPE1_FIELD_MODULE_ID,
				   0, str);
	}

	if (err->val_bits & HISI_OEM_VALID_SUB_MODULE_ID &&
	    IN_RANGE(p, buf, end)) {
		const char *str = oem_submodule_name(hisi_oem_type1_module,
						     err->module_id,
						     err->sub_module_id);

		p += snprintf(p, end - p, "submodule=%s ", str);
		record_vendor_data(dec_tab, HISI_OEM_DATA_TYPE_TEXT,
				   HIP08_OEM_TYPE1_FIELD_SUB_MODULE_ID,
				   0, str);
	}

	if (err->val_bits & HISI_OEM_VALID_ERR_SEVERITY &&
	    IN_RANGE(p, buf, end)) {
		p += snprintf(p, end - p, "error_severity=%s ",
			     err_severity(err->err_severity));
		record_vendor_data(dec_tab, HISI_OEM_DATA_TYPE_TEXT,
				   HIP08_OEM_TYPE1_FIELD_ERR_SEV,
				   0, err_severity(err->err_severity));
	}

	if (IN_RANGE(p, buf, end))
		p += snprintf(p, end - p, "]");

	trace_seq_printf(s, "%s\n", buf);
}

static void decode_oem_type1_err_regs(struct ras_ns_dec_tab *dec_tab,
				      struct trace_seq *s,
				      const struct hisi_oem_type1_err_sec *err)
{
	char buf[HISI_BUF_LEN];
	char *p = buf;
	char *end = buf + HISI_BUF_LEN;

	trace_seq_printf(s, "Reg Dump:\n");
	if (err->val_bits & HISI_OEM_TYPE1_VALID_ERR_MISC_0) {
		trace_seq_printf(s, "ERR_MISC0=0x%x\n", err->err_misc_0);
		p += snprintf(p, end - p, "ERR_MISC0=0x%x ", err->err_misc_0);
	}

	if (err->val_bits & HISI_OEM_TYPE1_VALID_ERR_MISC_1 &&
	    IN_RANGE(p, buf, end)) {
		trace_seq_printf(s, "ERR_MISC1=0x%x\n", err->err_misc_1);
		p += snprintf(p, end - p, "ERR_MISC1=0x%x ", err->err_misc_1);
	}

	if (err->val_bits & HISI_OEM_TYPE1_VALID_ERR_MISC_2 &&
	    IN_RANGE(p, buf, end)) {
		trace_seq_printf(s, "ERR_MISC2=0x%x\n", err->err_misc_2);
		p += snprintf(p, end - p, "ERR_MISC2=0x%x ", err->err_misc_2);
	}

	if (err->val_bits & HISI_OEM_TYPE1_VALID_ERR_MISC_3 &&
	    IN_RANGE(p, buf, end)) {
		trace_seq_printf(s, "ERR_MISC3=0x%x\n", err->err_misc_3);
		p += snprintf(p, end - p, "ERR_MISC3=0x%x ", err->err_misc_3);
	}

	if (err->val_bits & HISI_OEM_TYPE1_VALID_ERR_MISC_4 &&
	    IN_RANGE(p, buf, end)) {
		trace_seq_printf(s, "ERR_MISC4=0x%x\n", err->err_misc_4);
		p += snprintf(p, end - p, "ERR_MISC4=0x%x ", err->err_misc_4);
	}

	if (err->val_bits & HISI_OEM_TYPE1_VALID_ERR_ADDR &&
	    IN_RANGE(p, buf, end)) {
		trace_seq_printf(s, "ERR_ADDR=0x%llx\n",
				 (unsigned long long)err->err_addr);
		p += snprintf(p, end - p, "ERR_ADDR=0x%llx ",
			     (unsigned long long)err->err_addr);
	}

	if (p > buf && p < end) {
		p--;
		*p = '\0';
	}

	record_vendor_data(dec_tab, HISI_OEM_DATA_TYPE_TEXT,
			   HIP08_OEM_TYPE1_FIELD_REGS_DUMP, 0, buf);
	step_vendor_data_tab(dec_tab, "hip08_oem_type1_event_tab");
}

/* error data decoding functions */
static int decode_hip08_oem_type1_error(struct ras_events *ras,
					struct ras_ns_dec_tab *dec_tab,
					struct trace_seq *s,
					struct ras_non_standard_event *event)
{
	const struct hisi_oem_type1_err_sec *err =
			(struct hisi_oem_type1_err_sec*)event->error;

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
	record_vendor_data(dec_tab, HISI_OEM_DATA_TYPE_TEXT,
			   HIP08_OEM_TYPE1_FIELD_TIMESTAMP,
			   0, event->timestamp);

	trace_seq_printf(s, "\nHISI HIP08: OEM Type-1 Error\n");
	decode_oem_type1_err_hdr(dec_tab, s, err);
	decode_oem_type1_err_regs(dec_tab, s, err);

	return 0;
}

static void decode_oem_type2_err_hdr(struct ras_ns_dec_tab *dec_tab,
				     struct trace_seq *s,
				     const struct hisi_oem_type2_err_sec *err)
{
	char buf[HISI_BUF_LEN];
	char *p = buf;
	char *end = buf + HISI_BUF_LEN;

	p += snprintf(p, end - p, "[ table_version=%d ", err->version);
	record_vendor_data(dec_tab, HISI_OEM_DATA_TYPE_INT,
			   HIP08_OEM_TYPE2_FIELD_VERSION, err->version, NULL);

	if (err->val_bits & HISI_OEM_VALID_SOC_ID && IN_RANGE(p, buf, end)) {
		p += snprintf(p, end - p, "SOC_ID=%d ", err->soc_id);
		record_vendor_data(dec_tab, HISI_OEM_DATA_TYPE_INT,
				   HIP08_OEM_TYPE2_FIELD_SOC_ID,
				   err->soc_id, NULL);
	}

	if (err->val_bits & HISI_OEM_VALID_SOCKET_ID && IN_RANGE(p, buf, end)) {
		p += snprintf(p, end - p, "socket_ID=%d ", err->socket_id);
		record_vendor_data(dec_tab, HISI_OEM_DATA_TYPE_INT,
				   HIP08_OEM_TYPE2_FIELD_SOCKET_ID,
				   err->socket_id, NULL);
	}

	if (err->val_bits & HISI_OEM_VALID_NIMBUS_ID && IN_RANGE(p, buf, end)) {
		p += snprintf(p, end - p, "nimbus_ID=%d ", err->nimbus_id);
		record_vendor_data(dec_tab, HISI_OEM_DATA_TYPE_INT,
				   HIP08_OEM_TYPE2_FIELD_NIMBUS_ID,
				   err->nimbus_id, NULL);
	}

	if (err->val_bits & HISI_OEM_VALID_MODULE_ID && IN_RANGE(p, buf, end)) {
		const char *str = oem_module_name(hisi_oem_type2_module,
						  err->module_id);

		p += snprintf(p, end - p, "module=%s ", str);
		record_vendor_data(dec_tab, HISI_OEM_DATA_TYPE_TEXT,
				   HIP08_OEM_TYPE2_FIELD_MODULE_ID,
				   0, str);
	}

	if (err->val_bits & HISI_OEM_VALID_SUB_MODULE_ID &&
	    IN_RANGE(p, buf, end)) {
		const char *str = oem_submodule_name(hisi_oem_type2_module,
						     err->module_id,
						     err->sub_module_id);

		p += snprintf(p, end - p, "submodule=%s ", str);
		record_vendor_data(dec_tab, HISI_OEM_DATA_TYPE_TEXT,
				   HIP08_OEM_TYPE2_FIELD_SUB_MODULE_ID,
				   0, str);
	}

	if (err->val_bits & HISI_OEM_VALID_ERR_SEVERITY &&
	    IN_RANGE(p, buf, end)) {
		p += snprintf(p, end - p, "error_severity=%s ",
			     err_severity(err->err_severity));
		record_vendor_data(dec_tab, HISI_OEM_DATA_TYPE_TEXT,
				   HIP08_OEM_TYPE2_FIELD_ERR_SEV,
				   0, err_severity(err->err_severity));
	}

	if (IN_RANGE(p, buf, end))
		p += snprintf(p, end - p, "]");

	trace_seq_printf(s, "%s\n", buf);
}

static void decode_oem_type2_err_regs(struct ras_ns_dec_tab *dec_tab,
				      struct trace_seq *s,
				      const struct hisi_oem_type2_err_sec *err)
{
	char buf[HISI_BUF_LEN];
	char *p = buf;
	char *end = buf + HISI_BUF_LEN;

	trace_seq_printf(s, "Reg Dump:\n");
	if (err->val_bits & HISI_OEM_TYPE2_VALID_ERR_FR) {
		trace_seq_printf(s, "ERR_FR_0=0x%x\n", err->err_fr_0);
		trace_seq_printf(s, "ERR_FR_1=0x%x\n", err->err_fr_1);
		p += snprintf(p, end - p, "ERR_FR_0=0x%x ERR_FR_1=0x%x ",
			     err->err_fr_0, err->err_fr_1);
	}

	if (err->val_bits & HISI_OEM_TYPE2_VALID_ERR_CTRL &&
	    IN_RANGE(p, buf, end)) {
		trace_seq_printf(s, "ERR_CTRL_0=0x%x\n", err->err_ctrl_0);
		trace_seq_printf(s, "ERR_CTRL_1=0x%x\n", err->err_ctrl_1);
		p += snprintf(p, end - p, "ERR_CTRL_0=0x%x ERR_CTRL_1=0x%x ",
			      err->err_ctrl_0, err->err_ctrl_1);
	}

	if (err->val_bits & HISI_OEM_TYPE2_VALID_ERR_STATUS &&
	    IN_RANGE(p, buf, end)) {
		trace_seq_printf(s, "ERR_STATUS_0=0x%x\n", err->err_status_0);
		trace_seq_printf(s, "ERR_STATUS_1=0x%x\n", err->err_status_1);
		p += snprintf(p, end - p, "ERR_STATUS_0=0x%x ERR_STATUS_1=0x%x ",
			      err->err_status_0, err->err_status_1);
	}

	if (err->val_bits & HISI_OEM_TYPE2_VALID_ERR_ADDR &&
	    IN_RANGE(p, buf, end)) {
		trace_seq_printf(s, "ERR_ADDR_0=0x%x\n", err->err_addr_0);
		trace_seq_printf(s, "ERR_ADDR_1=0x%x\n", err->err_addr_1);
		p += snprintf(p, end - p, "ERR_ADDR_0=0x%x ERR_ADDR_1=0x%x ",
			      err->err_addr_0, err->err_addr_1);
	}

	if (err->val_bits & HISI_OEM_TYPE2_VALID_ERR_MISC_0 &&
	    IN_RANGE(p, buf, end)) {
		trace_seq_printf(s, "ERR_MISC0_0=0x%x\n", err->err_misc0_0);
		trace_seq_printf(s, "ERR_MISC0_1=0x%x\n", err->err_misc0_1);
		p += snprintf(p, end - p, "ERR_MISC0_0=0x%x ERR_MISC0_1=0x%x ",
			      err->err_misc0_0, err->err_misc0_1);
	}

	if (err->val_bits & HISI_OEM_TYPE2_VALID_ERR_MISC_1 &&
	    IN_RANGE(p, buf, end)) {
		trace_seq_printf(s, "ERR_MISC1_0=0x%x\n", err->err_misc1_0);
		trace_seq_printf(s, "ERR_MISC1_1=0x%x\n", err->err_misc1_1);
		p += snprintf(p, end - p, "ERR_MISC1_0=0x%x ERR_MISC1_1=0x%x ",
			      err->err_misc1_0, err->err_misc1_1);
	}

	if (p > buf && p < end) {
		p--;
		*p = '\0';
	}

	record_vendor_data(dec_tab, HISI_OEM_DATA_TYPE_TEXT,
			   HIP08_OEM_TYPE2_FIELD_REGS_DUMP, 0, buf);
	step_vendor_data_tab(dec_tab, "hip08_oem_type2_event_tab");
}

static int decode_hip08_oem_type2_error(struct ras_events *ras,
					struct ras_ns_dec_tab *dec_tab,
					struct trace_seq *s,
					struct ras_non_standard_event *event)
{
	const struct hisi_oem_type2_err_sec *err =
			(struct hisi_oem_type2_err_sec *)event->error;

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
	record_vendor_data(dec_tab, HISI_OEM_DATA_TYPE_TEXT,
			   HIP08_OEM_TYPE2_FIELD_TIMESTAMP,
			   0, event->timestamp);

	trace_seq_printf(s, "\nHISI HIP08: OEM Type-2 Error\n");
	decode_oem_type2_err_hdr(dec_tab, s, err);
	decode_oem_type2_err_regs(dec_tab, s, err);

	return 0;
}

static void decode_pcie_local_err_hdr(struct ras_ns_dec_tab *dec_tab,
				      struct trace_seq *s,
				      const struct hisi_pcie_local_err_sec *err)
{
	char buf[HISI_BUF_LEN];
	char *p = buf;
	char *end = buf + HISI_BUF_LEN;

	p += snprintf(p, end - p, "[ table_version=%d ", err->version);
	record_vendor_data(dec_tab, HISI_OEM_DATA_TYPE_INT,
			   HIP08_PCIE_LOCAL_FIELD_VERSION,
			   err->version, NULL);

	if (err->val_bits & HISI_PCIE_LOCAL_VALID_SOC_ID &&
	    IN_RANGE(p, buf, end)) {
		p += snprintf(p, end - p, "SOC_ID=%d ", err->soc_id);
		record_vendor_data(dec_tab, HISI_OEM_DATA_TYPE_INT,
				   HIP08_PCIE_LOCAL_FIELD_SOC_ID,
				   err->soc_id, NULL);
	}

	if (err->val_bits & HISI_PCIE_LOCAL_VALID_SOCKET_ID &&
	    IN_RANGE(p, buf, end)) {
		p += snprintf(p, end - p, "socket_ID=%d ", err->socket_id);
		record_vendor_data(dec_tab, HISI_OEM_DATA_TYPE_INT,
				   HIP08_PCIE_LOCAL_FIELD_SOCKET_ID,
				   err->socket_id, NULL);
	}

	if (err->val_bits & HISI_PCIE_LOCAL_VALID_NIMBUS_ID &&
	    IN_RANGE(p, buf, end)) {
		p += snprintf(p, end - p, "nimbus_ID=%d ", err->nimbus_id);
		record_vendor_data(dec_tab, HISI_OEM_DATA_TYPE_INT,
				   HIP08_PCIE_LOCAL_FIELD_NIMBUS_ID,
				   err->nimbus_id, NULL);
	}

	if (err->val_bits & HISI_PCIE_LOCAL_VALID_SUB_MODULE_ID &&
	    IN_RANGE(p, buf, end)) {
		p += snprintf(p, end - p, "submodule=%s ",
			      pcie_local_sub_module_name(err->sub_module_id));
		record_vendor_data(dec_tab, HISI_OEM_DATA_TYPE_TEXT,
				   HIP08_PCIE_LOCAL_FIELD_SUB_MODULE_ID,
				   0, pcie_local_sub_module_name(err->sub_module_id));
	}

	if (err->val_bits & HISI_PCIE_LOCAL_VALID_CORE_ID &&
	    IN_RANGE(p, buf, end)) {
		p += snprintf(p, end - p, "core_ID=core%d ", err->core_id);
		record_vendor_data(dec_tab, HISI_OEM_DATA_TYPE_INT,
				   HIP08_PCIE_LOCAL_FIELD_CORE_ID,
				   err->core_id, NULL);
	}

	if (err->val_bits & HISI_PCIE_LOCAL_VALID_PORT_ID &&
	    IN_RANGE(p, buf, end)) {
		p += snprintf(p, end - p, "port_ID=port%d ", err->port_id);
		record_vendor_data(dec_tab, HISI_OEM_DATA_TYPE_INT,
				   HIP08_PCIE_LOCAL_FIELD_PORT_ID,
				   err->port_id, NULL);
	}

	if (err->val_bits & HISI_PCIE_LOCAL_VALID_ERR_SEVERITY &&
	    IN_RANGE(p, buf, end)) {
		p += snprintf(p, end - p, "error_severity=%s ",
			      err_severity(err->err_severity));
		record_vendor_data(dec_tab, HISI_OEM_DATA_TYPE_TEXT,
				   HIP08_PCIE_LOCAL_FIELD_ERR_SEV,
				   0, err_severity(err->err_severity));
	}

	if (err->val_bits & HISI_PCIE_LOCAL_VALID_ERR_TYPE &&
	    IN_RANGE(p, buf, end)) {
		p += snprintf(p, end - p, "error_type=0x%x ", err->err_type);
		record_vendor_data(dec_tab, HISI_OEM_DATA_TYPE_INT,
				   HIP08_PCIE_LOCAL_FIELD_ERR_TYPE,
				   err->err_type, NULL);
	}

	if (IN_RANGE(p, buf, end))
		p += snprintf(p, end - p, "]");

	trace_seq_printf(s, "%s\n", buf);
}

static void decode_pcie_local_err_regs(struct ras_ns_dec_tab *dec_tab,
				       struct trace_seq *s,
				       const struct hisi_pcie_local_err_sec *err)
{
	char buf[HISI_BUF_LEN];
	char *p = buf;
	char *end = buf + HISI_BUF_LEN;
	uint32_t i;

	trace_seq_printf(s, "Reg Dump:\n");
	for (i = 0; i < HISI_PCIE_LOCAL_ERR_MISC_MAX; i++) {
		if (err->val_bits & BIT(HISI_PCIE_LOCAL_VALID_ERR_MISC + i) &&
		    IN_RANGE(p, buf, end)) {
			trace_seq_printf(s, "ERR_MISC_%d=0x%x\n", i,
					 err->err_misc[i]);
			p += snprintf(p, end - p, "ERR_MISC_%d=0x%x ",
				      i, err->err_misc[i]);
		}
	}

	if (p > buf && p < end) {
		p--;
		*p = '\0';
	}

	record_vendor_data(dec_tab, HISI_OEM_DATA_TYPE_TEXT,
			   HIP08_PCIE_LOCAL_FIELD_REGS_DUMP, 0, buf);
	step_vendor_data_tab(dec_tab, "hip08_pcie_local_event_tab");
}

static int decode_hip08_pcie_local_error(struct ras_events *ras,
					 struct ras_ns_dec_tab *dec_tab,
					 struct trace_seq *s,
					 struct ras_non_standard_event *event)
{
	const struct hisi_pcie_local_err_sec *err =
			(struct hisi_pcie_local_err_sec *)event->error;

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
	record_vendor_data(dec_tab, HISI_OEM_DATA_TYPE_TEXT,
			   HIP08_PCIE_LOCAL_FIELD_TIMESTAMP,
			   0, event->timestamp);

	trace_seq_printf(s, "\nHISI HIP08: PCIe local error\n");
	decode_pcie_local_err_hdr(dec_tab, s, err);
	decode_pcie_local_err_regs(dec_tab, s, err);

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

static void __attribute__((constructor)) hip08_init(void)
{
	register_ns_dec_tab(hip08_ns_oem_tab);
}
