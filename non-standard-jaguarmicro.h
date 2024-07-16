/*
 * Copyright (c) 2023, JaguarMicro
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */

#ifndef __NON_STANDARD_JAGUAR_H
#define __NON_STANDARD_JAGUAR_H

#include "ras-events.h"
#include <traceevent/event-parse.h>
#include "ras-mce-handler.h"

#define PAYLOAD_TYPE_0         0x00
#define PAYLOAD_TYPE_1         0x01
#define PAYLOAD_TYPE_2         0x02
#define PAYLOAD_TYPE_3         0x03
#define PAYLOAD_TYPE_4         0x04
#define PAYLOAD_TYPE_5         0x05
#define PAYLOAD_TYPE_6         0x06
#define PAYLOAD_TYPE_7         0x07
#define PAYLOAD_TYPE_8         0x08
#define PAYLOAD_TYPE_9         0x09

#define PAYLOAD_VERSION        0

enum {
	JM_COMMON_VALID_VERSION = 0,
	JM_COMMON_VALID_SOC_ID,
	JM_COMMON_VALID_SUBSYSTEM_ID,
	JM_COMMON_VALID_MODULE_ID,
	JM_COMMON_VALID_SUBMODULE_ID,
	JM_COMMON_VALID_DEV_ID,
	JM_COMMON_VALID_ERR_TYPE,
	JM_COMMON_VALID_ERR_SEVERITY,
	JM_COMMON_VALID_REG_ARRAY_SIZE = 11,
};

struct jm_common_sec_head {
	uint32_t val_bits;
	uint8_t  version;
	uint8_t  soc_id;
	uint8_t  subsystem_id;
	uint8_t  module_id;
	uint8_t  submodule_id;
	uint8_t  dev_id;
	uint16_t err_type;
	uint8_t  err_severity;
	uint8_t  reserved[3];
};

struct jm_common_sec_tail {
	uint32_t reg_array_size;
	uint32_t reg_array[];
};

/* ras_csr_por*/
struct jm_payload0_type_sec {
	struct jm_common_sec_head common_head;

	uint32_t lock_control;
	uint32_t lock_function;
	uint32_t cfg_ram_id;
	uint32_t err_fr_low32;
	uint32_t err_fr_high32;
	uint32_t err_ctlr_low32;
	uint32_t ecc_status_low32;
	uint32_t ecc_addr_low32;
	uint32_t ecc_addr_high32;
	uint32_t ecc_misc0_low32;
	uint32_t ecc_misc0_high32;
	uint32_t ecc_misc1_low32;
	uint32_t ecc_misc1_high32;
	uint32_t ecc_misc2_Low32;
	uint32_t ecc_misc2_high32;

	struct jm_common_sec_tail common_tail;
};

/*SMMU IP*/
struct jm_payload1_type_sec {
	struct jm_common_sec_head common_head;

	uint32_t smmu_csr;
	uint32_t errfr;
	uint32_t errctlr;
	uint32_t errstatus;
	uint32_t errgen;

	struct jm_common_sec_tail common_tail;
};

/*HAC SRAM */
struct jm_payload2_type_sec {
	struct jm_common_sec_head common_head;

	uint32_t ecc_1bit_int_low;
	uint32_t ecc_1bit_int_high;
	uint32_t ecc_2bit_int_low;
	uint32_t ecc_2bit_int_high;

	struct jm_common_sec_tail common_tail;
};

/*CMN IP */
struct jm_payload5_type_sec {
	struct jm_common_sec_head common_head;

	uint64_t cfgm_mxp_0;
	uint64_t cfgm_hnf_0;
	uint64_t cfgm_hni_0;
	uint64_t cfgm_sbsx_0;
	uint64_t errfr_NS;
	uint64_t errctlrr_NS;
	uint64_t errstatusr_NS;
	uint64_t erraddrr_NS;
	uint64_t errmiscr_NS;
	uint64_t errfr;
	uint64_t errctlr;
	uint64_t errstatus;
	uint64_t erraddr;
	uint64_t errmisc;

	struct jm_common_sec_tail common_tail;
};

/*GIC IP */
struct jm_payload6_type_sec {
	struct jm_common_sec_head common_head;
	uint64_t record_id;
	uint64_t gict_err_fr;
	uint64_t gict_err_ctlr;
	uint64_t gict_err_status;
	uint64_t gict_err_addr;
	uint64_t gict_err_misc0;
	uint64_t gict_err_misc1;
	uint64_t gict_errgsr;

	struct jm_common_sec_tail common_tail;
};

enum jm_oem_data_type {
	JM_OEM_DATA_TYPE_INT,
	JM_OEM_DATA_TYPE_INT64,
	JM_OEM_DATA_TYPE_TEXT,
};

enum {
	JM_PAYLOAD_FIELD_ID,
	JM_PAYLOAD_FIELD_TIMESTAMP,
	JM_PAYLOAD_FIELD_VERSION,
	JM_PAYLOAD_FIELD_SOC_ID,
	JM_PAYLOAD_FIELD_SUB_SYS,
	JM_PAYLOAD_FIELD_MODULE,
	JM_PAYLOAD_FIELD_MODULE_ID,
	JM_PAYLOAD_FIELD_SUB_MODULE,
	JM_PAYLOAD_FIELD_SUBMODULE_ID,
	JM_PAYLOAD_FIELD_DEV,
	JM_PAYLOAD_FIELD_DEV_ID,
	JM_PAYLOAD_FIELD_ERR_TYPE,
	JM_PAYLOAD_FIELD_ERR_SEVERITY,
	JM_PAYLOAD_FIELD_REGS_DUMP,
};

#define JM_SNPRINTF	mce_snprintf
#endif
