/* SPDX-License-Identifier: GPL-2.0-only */

/*
 * Copyright (c) 2025, Ampere Computing LLC
 */


#ifndef __NON_STANDARD_AMPEREONE_H
#define __NON_STANDARD_AMPEREONE_H

#include "ras-events.h"
#include <traceevent/event-parse.h>

#define AMPEREONE_SEVERITY(x) ((x >> 30) & 0x3)
#define AMPEREONE_SOCKET_NUM(x) ((x >> 29) & 0x1)
#define AMPEREONE_PAYLOAD_TYPE(x) ((x >> 12) & 0xf)
#define AMPEREONE_TYPE(x) (x & 0x7ff)
#define AMPEREONE_INSTANCE(x) (x & 0x7fff)
#define AMPEREONE_PAYLOAD0_BUF_LEN   1024
#define AMPEREONE_PAYLOAD1_BUF_LEN   2048
#define AMPEREONE_PAYLOAD2_BUF_LEN   1024
#define AMPEREONE_PAYLOAD3_BUF_LEN   1024
#define AMPEREONE_PAYLOAD4_BUF_LEN   1024
#define AMPEREONE_PAYLOAD5_BUF_LEN   1024
#define AMPEREONE_PAYLOAD6_BUF_LEN   1024
#define AMPEREONE_PAYLOAD_TYPE_0         0x00
#define AMPEREONE_PAYLOAD_TYPE_1         0x01
#define AMPEREONE_PAYLOAD_TYPE_2         0x02
#define AMPEREONE_PAYLOAD_TYPE_3         0x03
#define AMPEREONE_PAYLOAD_TYPE_4         0x04
#define AMPEREONE_PAYLOAD_TYPE_5         0x05
#define AMPEREONE_PAYLOAD_TYPE_6         0x06


/* AmpereOne RAS Error type definitions */
#define AMPEREONE_RAS_TYPE_CPU			0
#define AMPEREONE_RAS_TYPE_MCU			1
#define AMPEREONE_RAS_TYPE_CMN			2
#define AMPEREONE_RAS_TYPE_2P_LINK_AER	3
#define AMPEREONE_RAS_TYPE_2P_LINK_ALI	4
#define AMPEREONE_RAS_TYPE_GIC			5
#define AMPEREONE_RAS_TYPE_SMMU			6
#define AMPEREONE_RAS_TYPE_PCIE_AER		7
#define AMPEREONE_RAS_TYPE_PCIE_HB		8
#define AMPEREONE_RAS_TYPE_PCIE_RASDP	9
#define AMPEREONE_RAS_TYPE_OCM			10
#define AMPEREONE_RAS_TYPE_MPRO			11
#define AMPEREONE_RAS_TYPE_SECPRO		12
#define AMPEREONE_RAS_TYPE_INTERNAL_SOC	13
#define AMPEREONE_RAS_TYPE_MPRO_FW		14
#define AMPEREONE_RAS_TYPE_SECPRO_FW	15
#define AMPEREONE_RAS_TYPE_BERT			31

struct ampereone_payload_header {
	uint16_t type;
	uint16_t subtype;
	uint32_t instance;
};

// Payload Type 0: ARMv8 RAS Format
struct ampereone_payload0_type_sec {
	struct ampereone_payload_header header;
	uint64_t err_fr;
	uint64_t err_ctlr;
	uint64_t err_status;
	uint64_t err_addr;
	uint64_t err_misc_0;
	uint64_t err_misc_1;
	uint64_t err_misc_2;
	uint64_t err_misc_3;
};

// Payload Type 1: PCIe AER format
struct ampereone_payload1_type_sec {
	struct ampereone_payload_header header;
	uint32_t aer_ue_err_status;
	uint32_t aer_ce_err_status;
	uint64_t ebuf_overflow;
	uint64_t ebuf_underrun;
	uint64_t decode_error;
	uint64_t running_disparity_error;
	uint64_t skp_os_parity_error_gen3;
	uint64_t sync_header_error;
	uint64_t rx_valid_deassertion;
	uint64_t ctl_skp_os_parity_error_gen4;
	uint64_t first_retimer_parity_error_gen4;
	uint64_t second_retimer_parity_error_gen4;
	uint64_t margin_crc_and_parity_error_gen4;
	uint64_t rasdes_group1_counters : 48;
	uint64_t rsvd0 : 16;
	uint64_t rasdes_group2_counters;
	uint64_t ebuf_skp_add;
	uint64_t ebuf_skp_del;
	uint64_t rasdes_group5_counters;
	uint64_t rasdes_group5_counters_continued : 48;
	uint64_t rsvd1 : 16;
	uint32_t dbg_l1_status_lane0;
	uint32_t dbg_l1_status_lane1;
	uint32_t dbg_l1_status_lane2;
	uint32_t dbg_l1_status_lane3;
	uint32_t dbg_l1_status_lane4;
	uint32_t dbg_l1_status_lane5;
	uint32_t dbg_l1_status_lane6;
	uint32_t dbg_l1_status_lane7;
	uint32_t dbg_l1_status_lane8;
	uint32_t dbg_l1_status_lane9;
	uint32_t dbg_l1_status_lane10;
	uint32_t dbg_l1_status_lane11;
	uint32_t dbg_l1_status_lane12;
	uint32_t dbg_l1_status_lane13;
	uint32_t dbg_l1_status_lane14;
	uint32_t dbg_l1_status_lane15;
};

// Payload Type 2: PCIe RAS Data Path (RASDP) Format
struct ampereone_payload2_type_sec {
	struct ampereone_payload_header header;
	uint32_t corr_count_report;
	uint32_t corr_error_location;
	uint32_t ram_addr_corr;
	uint32_t uncorr_count_report;
	uint32_t uncorr_error_location;
	uint32_t ram_addr_uncorr;
};

// Payload Type 3: MCU ECC
struct ampereone_payload3_type_sec {
	struct ampereone_payload_header header;
	uint64_t ecc_addr;
	uint64_t ecc_data;
	uint32_t ecc_id;
	uint32_t ecc_synd;
	uint32_t ecc_mce_cnt;
	uint32_t ecc_ctlr;
	uint32_t ecc_err_sts;
	uint32_t ecc_err_cnt;
};

// Payload Type 4: MCU CHI
struct ampereone_payload4_type_sec {
	struct ampereone_payload_header header;
	uint64_t address;
	uint32_t srcid;
	uint32_t txnid;
	uint32_t type;
	uint32_t lpid;
	uint32_t opcode;
	uint32_t tag;
	uint32_t mpam;
	uint32_t reserved;
};

// Payload Type 5: BERT
struct ampereone_payload5_type_sec {
	struct ampereone_payload_header header;
};

// Payload Type 6: FW Error
struct ampereone_payload6_type_sec {
	struct ampereone_payload_header header;
	uint8_t driver;
	uint32_t error_code;
	uint8_t error_msg_size;
	uint8_t error_msg[];
};

enum ampereone_oem_data_type {
	AMPEREONE_OEM_DATA_TYPE_INT,
	AMPEREONE_OEM_DATA_TYPE_INT64,
	AMPEREONE_OEM_DATA_TYPE_TEXT,
};

enum {
	AMPEREONE_PAYLOAD0_FIELD_ID,
	AMPEREONE_PAYLOAD0_FIELD_TIMESTAMP,
	AMPEREONE_PAYLOAD0_FIELD_TYPE_ID,
	AMPEREONE_PAYLOAD0_FIELD_SUB_TYPE_ID,
	AMPEREONE_PAYLOAD0_FIELD_INS,
	AMPEREONE_PAYLOAD0_FIELD_SOCKET_NUM,
	AMPEREONE_PAYLOAD0_FIELD_ERRxFR,
	AMPEREONE_PAYLOAD0_FIELD_ERRxCTLR,
	AMPEREONE_PAYLOAD0_FIELD_ERRxSTATUS,
	AMPEREONE_PAYLOAD0_FIELD_ERRxADDR,
	AMPEREONE_PAYLOAD0_FIELD_ERRxMISC0,
	AMPEREONE_PAYLOAD0_FIELD_ERRxMISC1,
	AMPEREONE_PAYLOAD0_FIELD_ERRxMISC2,
	AMPEREONE_PAYLOAD0_FIELD_ERRxMISC3,
};

enum {
	AMPEREONE_PAYLOAD1_FIELD_ID,
	AMPEREONE_PAYLOAD1_FIELD_TIMESTAMP,
	AMPEREONE_PAYLOAD1_FIELD_TYPE_ID,
	AMPEREONE_PAYLOAD1_FIELD_SUB_TYPE_ID,
	AMPEREONE_PAYLOAD1_FIELD_INS,
	AMPEREONE_PAYLOAD1_FIELD_SOCKET_NUM,
	AMPEREONE_PAYLOAD1_FIELD_AER_CORR_ERR_STATUS,
	AMPEREONE_PAYLOAD1_FIELD_AER_UNCORR_ERR_STATUS,
	AMPEREONE_PAYLOAD1_FIELD_EBUF_OVERFLOW,
	AMPEREONE_PAYLOAD1_FIELD_EBUF_UNDERRUN,
	AMPEREONE_PAYLOAD1_FIELD_DECODE_ERROR,
	AMPEREONE_PAYLOAD1_FIELD_RUNNING_DISPARITY_ERROR,
	AMPEREONE_PAYLOAD1_FIELD_SKP_OS_PARITY_ERROR_GEN3,
	AMPEREONE_PAYLOAD1_FIELD_SYNC_HEADER_ERROR,
	AMPEREONE_PAYLOAD1_FIELD_RX_VALID_DEASSERTION,
	AMPEREONE_PAYLOAD1_FIELD_CTL_SKP_OS_PARITY_ERROR_GEN4,
	AMPEREONE_PAYLOAD1_FIELD_FIRST_RETIMER_PARITY_ERROR_GEN4,
	AMPEREONE_PAYLOAD1_FIELD_SECOND_RETIMER_PARITY_ERROR_GEN4,
	AMPEREONE_PAYLOAD1_FIELD_MARGIN_CRC_AND_PARTIY_ERROR_GEN4,
	AMPEREONE_PAYLOAD1_FIELD_RASDES_GROUP1_COUNTERS,
	AMPEREONE_PAYLOAD1_FIELD_RSVD0,
	AMPEREONE_PAYLOAD1_FIELD_RASDES_GROUP2_COUNTERS,
	AMPEREONE_PAYLOAD1_FIELD_EBUF_SKP_ADD,
	AMPEREONE_PAYLOAD1_FIELD_EBUF_SKP_DEL,
	AMPEREONE_PAYLOAD1_FIELD_RASDES_GROUP5_COUNTERS,
	AMPEREONE_PAYLOAD1_FIELD_RASDES_GROUP5_COUNTERS_CONTINUED,
	AMPEREONE_PAYLOAD1_FIELD_RSVD1,
	AMPEREONE_PAYLOAD1_FIELD_SI_DEBUG_LAYER1_STATUS_LANE0,
	AMPEREONE_PAYLOAD1_FIELD_SI_DEBUG_LAYER1_STATUS_LANE1,
	AMPEREONE_PAYLOAD1_FIELD_SI_DEBUG_LAYER1_STATUS_LANE2,
	AMPEREONE_PAYLOAD1_FIELD_SI_DEBUG_LAYER1_STATUS_LANE3,
	AMPEREONE_PAYLOAD1_FIELD_SI_DEBUG_LAYER1_STATUS_LANE4,
	AMPEREONE_PAYLOAD1_FIELD_SI_DEBUG_LAYER1_STATUS_LANE5,
	AMPEREONE_PAYLOAD1_FIELD_SI_DEBUG_LAYER1_STATUS_LANE6,
	AMPEREONE_PAYLOAD1_FIELD_SI_DEBUG_LAYER1_STATUS_LANE7,
	AMPEREONE_PAYLOAD1_FIELD_SI_DEBUG_LAYER1_STATUS_LANE8,
	AMPEREONE_PAYLOAD1_FIELD_SI_DEBUG_LAYER1_STATUS_LANE9,
	AMPEREONE_PAYLOAD1_FIELD_SI_DEBUG_LAYER1_STATUS_LANE10,
	AMPEREONE_PAYLOAD1_FIELD_SI_DEBUG_LAYER1_STATUS_LANE11,
	AMPEREONE_PAYLOAD1_FIELD_SI_DEBUG_LAYER1_STATUS_LANE12,
	AMPEREONE_PAYLOAD1_FIELD_SI_DEBUG_LAYER1_STATUS_LANE13,
	AMPEREONE_PAYLOAD1_FIELD_SI_DEBUG_LAYER1_STATUS_LANE14,
	AMPEREONE_PAYLOAD1_FIELD_SI_DEBUG_LAYER1_STATUS_LANE15,
};

enum {
	AMPEREONE_PAYLOAD2_FIELD_ID,
	AMPEREONE_PAYLOAD2_FIELD_TIMESTAMP,
	AMPEREONE_PAYLOAD2_FIELD_TYPE_ID,
	AMPEREONE_PAYLOAD2_FIELD_SUB_TYPE_ID,
	AMPEREONE_PAYLOAD2_FIELD_INS,
	AMPEREONE_PAYLOAD2_FIELD_SOCKET_NUM,
	AMPEREONE_PAYLOAD2_FIELD_CORR_COUNT_REPORT,
	AMPEREONE_PAYLOAD2_FIELD_CORR_ERROR_LOCATION,
	AMPEREONE_PAYLOAD2_FIELD_RAM_ADDR_CORR,
	AMPEREONE_PAYLOAD2_FIELD_UNCORR_COUNT_REPORT,
	AMPEREONE_PAYLOAD2_FIELD_UNCORR_ERROR_LOCATION,
	AMPEREONE_PAYLOAD2_FIELD_RAM_ADDR_UNCORR,
};

enum {
	AMPEREONE_PAYLOAD3_FIELD_ID,
	AMPEREONE_PAYLOAD3_FIELD_TIMESTAMP,
	AMPEREONE_PAYLOAD3_FIELD_TYPE_ID,
	AMPEREONE_PAYLOAD3_FIELD_SUB_TYPE_ID,
	AMPEREONE_PAYLOAD3_FIELD_INS,
	AMPEREONE_PAYLOAD3_FIELD_SOCKET_NUM,
	AMPEREONE_PAYLOAD3_FIELD_ECC_ADDRESS,
	AMPEREONE_PAYLOAD3_FIELD_ECC_DATA,
	AMPEREONE_PAYLOAD3_FIELD_ECC_SRC_ID,
	AMPEREONE_PAYLOAD3_FIELD_ECC_SYND,
	AMPEREONE_PAYLOAD3_FIELD_ECC_MCE_CNT,
	AMPEREONE_PAYLOAD3_FIELD_ECC_CTLR,
	AMPEREONE_PAYLOAD3_FIELD_ECC_ERR_STS,
	AMPEREONE_PAYLOAD3_FIELD_ECC_ERR_CNT,
};

enum {
	AMPEREONE_PAYLOAD4_FIELD_ID,
	AMPEREONE_PAYLOAD4_FIELD_TIMESTAMP,
	AMPEREONE_PAYLOAD4_FIELD_TYPE_ID,
	AMPEREONE_PAYLOAD4_FIELD_SUB_TYPE_ID,
	AMPEREONE_PAYLOAD4_FIELD_INS,
	AMPEREONE_PAYLOAD4_FIELD_SOCKET_NUM,
	AMPEREONE_PAYLOAD4_FIELD_ADDRESS,
	AMPEREONE_PAYLOAD4_FIELD_SRC_ID,
	AMPEREONE_PAYLOAD4_FIELD_TXNID,
	AMPEREONE_PAYLOAD4_FIELD_TYPE,
	AMPEREONE_PAYLOAD4_FIELD_LPID,
	AMPEREONE_PAYLOAD4_FIELD_OPCODE,
	AMPEREONE_PAYLOAD4_FIELD_TAG,
	AMPEREONE_PAYLOAD4_FIELD_MPAM,
};

enum {
	AMPEREONE_PAYLOAD5_FIELD_ID,
	AMPEREONE_PAYLOAD5_FIELD_TIMESTAMP,
	AMPEREONE_PAYLOAD5_FIELD_TYPE_ID,
	AMPEREONE_PAYLOAD5_FIELD_SUB_TYPE_ID,
	AMPEREONE_PAYLOAD5_FIELD_INS,
	AMPEREONE_PAYLOAD5_FIELD_SOCKET_NUM,
};

enum {
	AMPEREONE_PAYLOAD6_FIELD_ID,
	AMPEREONE_PAYLOAD6_FIELD_TIMESTAMP,
	AMPEREONE_PAYLOAD6_FIELD_TYPE_ID,
	AMPEREONE_PAYLOAD6_FIELD_SUB_TYPE_ID,
	AMPEREONE_PAYLOAD6_FIELD_INS,
	AMPEREONE_PAYLOAD6_FIELD_SOCKET_NUM,
	AMPEREONE_PAYLOAD6_FIELD_DRIVER,
	AMPEREONE_PAYLOAD6_FIELD_ERROR_CODE,
	AMPEREONE_PAYLOAD6_FIELD_MSG_SIZE,
	AMPEREONE_PAYLOAD6_FIELD_ERROR_MSG,
};
struct ras_non_standard_event;

int decode_ampereone_type_error(struct ras_events *ras,
				     struct ras_ns_ev_decoder *ev_decoder,
				     struct trace_seq *s,
				     struct ras_non_standard_event *event);

void decode_ampereone_payload0_err_regs(struct ras_ns_ev_decoder *ev_decoder,
				  struct trace_seq *s,
				  const struct ampereone_payload0_type_sec *err);

void decode_ampereone_payload1_err_regs(struct ras_ns_ev_decoder *ev_decoder,
				struct trace_seq *s,
				const struct ampereone_payload1_type_sec *err);

void decode_ampereone_payload2_err_regs(struct ras_ns_ev_decoder *ev_decoder,
				struct trace_seq *s,
				const struct ampereone_payload2_type_sec *err);

void decode_ampereone_payload3_err_regs(struct ras_ns_ev_decoder *ev_decoder,
				struct trace_seq *s,
				const struct ampereone_payload3_type_sec *err);

void decode_ampereone_payload4_err_regs(struct ras_ns_ev_decoder *ev_decoder,
				struct trace_seq *s,
				const struct ampereone_payload4_type_sec *err);

void decode_ampereone_payload5_err_regs(struct ras_ns_ev_decoder *ev_decoder,
				struct trace_seq *s,
				const struct ampereone_payload5_type_sec *err);

void decode_ampereone_payload6_err_regs(struct ras_ns_ev_decoder *ev_decoder,
				struct trace_seq *s,
				const struct ampereone_payload6_type_sec *err);

#endif
