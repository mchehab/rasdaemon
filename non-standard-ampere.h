/*
 * Copyright (c) 2020, Ampere Computing LLC.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */

#ifndef __NON_STANDARD_AMPERE_H
#define __NON_STANDARD_AMPERE_H

#include "ras-events.h"
#include <traceevent/event-parse.h>

#define SOCKET_NUM(x) ((x >> 14) & 0x3)
#define PAYLOAD_TYPE(x) ((x >> 6) & 0x3)
#define TYPE(x) (x & 0x3f)
#define INSTANCE(x) (x & 0x3fff)
#define AMP_PAYLOAD0_BUF_LEN   1024
#define PAYLOAD_TYPE_0         0x00
#define PAYLOAD_TYPE_1         0x01
#define PAYLOAD_TYPE_2         0x02
#define PAYLOAD_TYPE_3         0x03

/* Ampere RAS Error type definitions */
#define AMP_RAS_TYPE_CPU		0
#define AMP_RAS_TYPE_MCU		1
#define AMP_RAS_TYPE_MESH		2
#define AMP_RAS_TYPE_2P_LINK_QS	3
#define AMP_RAS_TYPE_2P_LINK_MQ	4
#define AMP_RAS_TYPE_GIC		5
#define AMP_RAS_TYPE_SMMU		6
#define AMP_RAS_TYPE_PCIE_AER		7
#define AMP_RAS_TYPE_PCIE_RASDP	8
#define AMP_RAS_TYPE_OCM		9
#define AMP_RAS_TYPE_SMPRO		10
#define AMP_RAS_TYPE_PMPRO		11
#define AMP_RAS_TYPE_ATF_FW		12
#define AMP_RAS_TYPE_SMPRO_FW		13
#define AMP_RAS_TYPE_PMPRO_FW		14
#define AMP_RAS_TYPE_BERT		63

/* ARMv8 RAS Compliant Error Record(APEI and BMC Reporting)*/
struct amp_payload0_type_sec {
	uint8_t    type;
	uint8_t    subtype;
	uint16_t   instance;
	uint32_t   err_status;
	uint64_t   err_addr;
	uint64_t   err_misc_0;
	uint64_t   err_misc_1;
	uint64_t   err_misc_2;
	uint64_t   err_misc_3;
};

/*PCIe AER format*/
struct amp_payload1_type_sec {
	uint8_t    type;
	uint8_t    subtype;
	uint16_t   instance;
	uint32_t   uncore_status;
	uint32_t   uncore_mask;
	uint32_t   uncore_sev;
	uint32_t   core_status;
	uint32_t   core_mask;
	uint32_t   root_err_cmd;
	uint32_t   root_status;
	uint32_t   src_id;
	uint32_t   reserved1;
	uint64_t   reserved2;
};

/*PCIe RAS Data Path(RASDP) format */
struct amp_payload2_type_sec {
	uint8_t    type;
	uint8_t    subtype;
	uint16_t   instance;
	uint32_t   ce_register;
	uint32_t   ce_location;
	uint32_t   ce_addr;
	uint32_t   ue_register;
	uint32_t   ue_location;
	uint32_t   ue_addr;
	uint32_t   reserved1;
	uint64_t   reserved2;
	uint64_t   reserved3;
};

/*Firmware-Specific Data(ATF,SMPro, and BERT) */
struct amp_payload3_type_sec {
	uint8_t    type;
	uint8_t    subtype;
	uint16_t   instance;
	uint32_t   fw_speci_data0;
	uint64_t   fw_speci_data1;
	uint64_t   fw_speci_data2;
	uint64_t   fw_speci_data3;
	uint64_t   fw_speci_data4;
	uint64_t   fw_speci_data5;
};

enum amp_oem_data_type {
	AMP_OEM_DATA_TYPE_INT,
	AMP_OEM_DATA_TYPE_INT64,
	AMP_OEM_DATA_TYPE_TEXT,
};

enum {
	AMP_PAYLOAD0_FIELD_ID,
	AMP_PAYLOAD0_FIELD_TIMESTAMP,
	AMP_PAYLOAD0_FIELD_TYPE,
	AMP_PAYLOAD0_FIELD_SUB_TYPE,
	AMP_PAYLOAD0_FIELD_INS,
	AMP_PAYLOAD0_FIELD_SOCKET_NUM,
	AMP_PAYLOAD0_FIELD_STATUS_REG,
	AMP_PAYLOAD0_FIELD_ADDR_REG,
	AMP_PAYLOAD0_FIELD_MISC0,
	AMP_PAYLOAD0_FIELD_MISC1,
	AMP_PAYLOAD0_FIELD_MISC2,
	AMP_PAYLOAD0_FIELD_MISC3,
};

enum {
	AMP_PAYLOAD1_FIELD_ID,
	AMP_PAYLOAD1_FIELD_TIMESTAMP,
	AMP_PAYLOAD1_FIELD_TYPE,
	AMP_PAYLOAD1_FIELD_SUB_TYPE,
	AMP_PAYLOAD1_FIELD_INS,
	AMP_PAYLOAD1_FIELD_SOCKET_NUM,
	AMP_PAYLOAD1_FIELD_UNCORE_ERR_STATUS,
	AMP_PAYLOAD1_FIELD_UNCORE_ERR_MASK,
	AMP_PAYLOAD1_FIELD_UNCORE_ERR_SEV,
	AMP_PAYLOAD1_FIELD_CORE_ERR_STATUS,
	AMP_PAYLOAD1_FIELD_CORE_ERR_MASK,
	AMP_PAYLOAD1_FIELD_ROOT_ERR_CMD,
	AMP_PAYLOAD1_FIELD_ROOT_ERR_STATUS,
	AMP_PAYLOAD1_FIELD_SRC_ID,
	AMP_PAYLOAD1_FIELD_RESERVED1,
	AMP_PAYLOAD1_FIELD_RESERVED2,
};

enum {
	AMP_PAYLOAD2_FIELD_ID,
	AMP_PAYLOAD2_FIELD_TIMESTAMP,
	AMP_PAYLOAD2_FIELD_TYPE,
	AMP_PAYLOAD2_FIELD_SUB_TYPE,
	AMP_PAYLOAD2_FIELD_INS,
	AMP_PAYLOAD2_FIELD_SOCKET_NUM,
	AMP_PAYLOAD2_FIELD_CE_REPORT_REG,
	AMP_PAYLOAD2_FIELD_CE_LOACATION,
	AMP_PAYLOAD2_FIELD_CE_ADDR,
	AMP_PAYLOAD2_FIELD_UE_REPORT_REG,
	AMP_PAYLOAD2_FIELD_UE_LOCATION,
	AMP_PAYLOAD2_FIELD_UE_ADDR,
	AMP_PAYLOAD2_FIELD_RESERVED1,
	AMP_PAYLOAD2_FIELD_RESERVED2,
	AMP_PAYLOAD2_FIELD_RESERVED3,
};

enum {
	AMP_PAYLOAD3_FIELD_ID,
	AMP_PAYLOAD3_FIELD_TIMESTAMP,
	AMP_PAYLOAD3_FIELD_TYPE,
	AMP_PAYLOAD3_FIELD_SUB_TYPE,
	AMP_PAYLOAD3_FIELD_INS,
	AMP_PAYLOAD3_FIELD_SOCKET_NUM,
	AMP_PAYLOAD3_FIELD_FW_SPEC_DATA0,
	AMP_PAYLOAD3_FIELD_FW_SPEC_DATA1,
	AMP_PAYLOAD3_FIELD_FW_SPEC_DATA2,
	AMP_PAYLOAD3_FIELD_FW_SPEC_DATA3,
	AMP_PAYLOAD3_FIELD_FW_SPEC_DATA4,
	AMP_PAYLOAD3_FIELD_FW_SPEC_DATA5
};

void decode_amp_payload0_err_regs(struct ras_ns_ev_decoder *ev_decoder,
				  struct trace_seq *s,
				  const struct amp_payload0_type_sec *err);

#endif
