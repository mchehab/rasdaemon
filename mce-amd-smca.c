/*
 * Copyright (c) 2018, AMD, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <stdio.h>
#include <string.h>

#include "ras-mce-handler.h"
#include "bitfield.h"

/* MCA_STATUS REGISTER FOR FAMILY 17H
 *********************** Higher 32-bits *****************************
 * 63: VALIDERROR, 62: OVERFLOW, 61: UC, 60: Err ENABLE,
 * 59: Misc Valid, 58: Addr Valid, 57: PCC, 56: ErrCoreID Valid,
 * 55: TCC, 54: RES, 53: Syndrom Valid, 52: Transparanet,
 * 51: RES, 50: RES, 49: RES, 48: RES,
 * 47: RES, 46: CECC, 45: UECC, 44: Deferred,
 * 43: Poison, 42: RES, 41: RES, 40: RES,
 * 39: RES, 38: RES, 37: ErrCoreID[5], 36: ErrCoreID[4],
 * 35: ErrCoreID[3], 34: ErrCoreID[2] 33: ErrCoreID[1] 32: ErrCoreID[0]
 *********************** Lower 32-bits ******************************
 * 31: RES, 30: RES, 29: RES, 28: RES,
 * 27: RES, 26: RES, 25: RES, 24: RES
 * 23: RES, 22: RES, 21: XEC[5], 20: XEC[4],
 * 19: XEC[3], 18: XEC[2], 17: XEC[1], 16: XEC[0]
 * 15: EC[15], 14: EC[14], 13: EC[13], 12: EC[12],
 * 11: EC[11], 10: EC[10], 09: EC[9], 08: EC[8],
 * 07: EC[7], 06: EC[6], 05: EC[5], 04: EC[4],
 * 03: EC[3], 02: EC[2], 01: EC[1], 00: EC[0]
 */

/* MCA_STATUS REGISTER FOR FAMILY 19H
 * The bits 24 ~ 29 contains AddressLsb
 * 29: ADDRLS[5], 28: ADDRLS[4], 27: ADDRLS[3],
 * 26: ADDRLS[2], 25: ADDRLS[1], 24: ADDRLS[0]
 */

/* These may be used by multiple smca_hwid_mcatypes */
enum smca_bank_types {
	SMCA_LS = 0,    /* Load Store */
	SMCA_LS_V2,
	SMCA_IF,        /* Instruction Fetch */
	SMCA_L2_CACHE,  /* L2 Cache */
	SMCA_DE,        /* Decoder Unit */
	SMCA_RESERVED,  /* Reserved */
	SMCA_EX,        /* Execution Unit */
	SMCA_FP,        /* Floating Point */
	SMCA_L3_CACHE,  /* L3 Cache */
	SMCA_CS,        /* Coherent Slave */
	SMCA_CS_V2,
	SMCA_PIE,       /* Power, Interrupts, etc. */
	SMCA_UMC,       /* Unified Memory Controller */
	SMCA_UMC_V2,
	SMCA_PB,        /* Parameter Block */
	SMCA_PSP,       /* Platform Security Processor */
	SMCA_PSP_V2,
	SMCA_SMU,       /* System Management Unit */
	SMCA_SMU_V2,
	SMCA_MP5,	/* Microprocessor 5 Unit */
	SMCA_NBIO,	/* Northbridge IO Unit */
	SMCA_PCIE,	/* PCI Express Unit */
	SMCA_PCIE_V2,
	SMCA_XGMI_PCS,	/* xGMI PCS Unit */
	SMCA_XGMI_PHY,	/* xGMI PHY Unit */
	SMCA_WAFL_PHY,	/* WAFL PHY Unit */
	N_SMCA_BANK_TYPES
};

/* Maximum number of MCA banks per CPU. */
#define MAX_NR_BANKS	64

/*
 * On Newer heterogeneous systems from AMD with CPU and GPU nodes connected
 * via xGMI links, the NON CPU Nodes are enumerated from index 8
 */
#define NONCPU_NODE_INDEX	8

/* SMCA Extended error strings */
/* Load Store */
static const char * const smca_ls_mce_desc[] = {
	"Load queue parity",
	"Store queue parity",
	"Miss address buffer payload parity",
	"L1 TLB parity",
	"Reserved",
	"DC tag error type 6",
	"DC tag error type 1",
	"Internal error type 1",
	"Internal error type 2",
	"Sys Read data error thread 0",
	"Sys read data error thread 1",
	"DC tag error type 2",
	"DC data error type 1 (poison consumption)",
	"DC data error type 2",
	"DC data error type 3",
	"DC tag error type 4",
	"L2 TLB parity",
	"PDC parity error",
	"DC tag error type 3",
	"DC tag error type 5",
	"L2 fill data error",
};
static const char * const smca_ls2_mce_desc[] = {
	"An ECC error was detected on a data cache read by a probe or victimization",
	"An ECC error or L2 poison was detected on a data cache read by a load",
	"An ECC error was detected on a data cache read-modify-write by a store",
	"An ECC error or poison bit mismatch was detected on a tag read by a probe or victimization",
	"An ECC error or poison bit mismatch was detected on a tag read by a load",
	"An ECC error or poison bit mismatch was detected on a tag read by a store",
	"An ECC error was detected on an EMEM read by a load",
	"An ECC error was detected on an EMEM read-modify-write by a store",
	"A parity error was detected in an L1 TLB entry by any access",
	"A parity error was detected in an L2 TLB entry by any access",
	"A parity error was detected in a PWC entry by any access",
	"A parity error was detected in an STQ entry by any access",
	"A parity error was detected in an LDQ entry by any access",
	"A parity error was detected in a MAB entry by any access",
	"A parity error was detected in an SCB entry state field by any access",
	"A parity error was detected in an SCB entry address field by any access",
	"A parity error was detected in an SCB entry data field by any access",
	"A parity error was detected in a WCB entry by any access",
	"A poisoned line was detected in an SCB entry by any access",
	"A SystemReadDataError error was reported on read data returned from L2 for a load",
	"A SystemReadDataError error was reported on read data returned from L2 for an SCB store",
	"A SystemReadDataError error was reported on read data returned from L2 for a WCB store",
	"A hardware assertion error was reported",
	"A parity error was detected in an STLF, SCB EMEM entry or SRB store data by any access",
};
/* Instruction Fetch */
static const char * const smca_if_mce_desc[] = {
	"microtag probe port parity error",
	"IC microtag or full tag multi-hit error",
	"IC full tag parity",
	"IC data array parity",
	"Decoupling queue phys addr parity error",
	"L0 ITLB parity error",
	"L1 ITLB parity error",
	"L2 ITLB parity error",
	"BPQ snoop parity on Thread 0",
	"BPQ snoop parity on Thread 1",
	"L1 BTB multi-match error",
	"L2 BTB multi-match error",
	"L2 Cache Response Poison error",
	"System Read Data error",
};
/* L2 Cache */
static const char * const smca_l2_mce_desc[] = {
	"L2M tag multi-way-hit error",
	"L2M tag ECC error",
	"L2M data ECC error",
	"HW assert",
};
/* Decoder Unit */
static const char * const smca_de_mce_desc[] = {
	"uop cache tag parity error",
	"uop cache data parity error",
	"Insn buffer parity error",
	"uop queue parity error",
	"Insn dispatch queue parity error",
	"Fetch address FIFO parity",
	"Patch RAM data parity",
	"Patch RAM sequencer parity",
	"uop buffer parity"
};
/* Execution Unit */
static const char * const smca_ex_mce_desc[] = {
	"Watchdog timeout error",
	"Phy register file parity",
	"Flag register file parity",
	"Immediate displacement register file parity",
	"Address generator payload parity",
	"EX payload parity",
	"Checkpoint queue parity",
	"Retire dispatch queue parity",
	"Retire status queue parity error",
	"Scheduling queue parity error",
	"Branch buffer queue parity error",
};
/* Floating Point Unit */
static const char * const smca_fp_mce_desc[] = {
	"Physical register file parity",
	"Freelist parity error",
	"Schedule queue parity",
	"NSQ parity error",
	"Retire queue parity",
	"Status register file parity",
	"Hardware assertion",
};
/* L3 Cache */
static const char * const smca_l3_mce_desc[] = {
	"Shadow tag macro ECC error",
	"Shadow tag macro multi-way-hit error",
	"L3M tag ECC error",
	"L3M tag multi-way-hit error",
	"L3M data ECC error",
	"XI parity, L3 fill done channel error",
	"L3 victim queue parity",
	"L3 HW assert",
};
/* Coherent Slave Unit */
static const char * const smca_cs_mce_desc[] = {
	"Illegal request from transport layer",
	"Address violation",
	"Security violation",
	"Illegal response from transport layer",
	"Unexpected response",
	"Parity error on incoming request or probe response data",
	"Parity error on incoming read response data",
	"Atomic request parity",
	"ECC error on probe filter access",
};
/* Coherent Slave Unit V2 */
static const char * const smca_cs2_mce_desc[] = {
	"Illegal Request",
	"Address Violation",
	"Security Violation",
	"Illegal Response",
	"Unexpected Response",
	"Request or Probe Parity Error",
	"Read Response Parity Error",
	"Atomic Request Parity Error",
	"SDP read response had no match in the CS queue",
	"Probe Filter Protocol Error",
	"Probe Filter ECC Error",
	"SDP read response had an unexpected RETRY error",
	"Counter overflow error",
	"Counter underflow error",
};
/* Power, Interrupt, etc.. */
static const char * const smca_pie_mce_desc[] = {
	"HW assert",
	"Internal PIE register security violation",
	"Error on GMI link",
	"Poison data written to internal PIE register",
};
/* Unified Memory Controller */
static const char * const smca_umc_mce_desc[] = {
	"DRAM ECC error",
	"Data poison error on DRAM",
	"SDP parity error",
	"Advanced peripheral bus error",
	"Command/address parity error",
	"Write data CRC error",
};

static const char * const smca_umc2_mce_desc[] = {
	"DRAM ECC error",
	"Data poison error",
	"SDP parity error",
	"Reserved",
	"Address/Command parity error",
	"Write data parity error",
	"DCQ SRAM ECC error",
	"Reserved",
	"Read data parity error",
	"Rdb SRAM ECC error",
	"RdRsp SRAM ECC error",
	"LM32 MP errors",
};

/* Parameter Block */
static const char * const smca_pb_mce_desc[] = {
	"Parameter Block RAM ECC error",
};
/* Platform Security Processor */
static const char * const smca_psp_mce_desc[] = {
	"PSP RAM ECC or parity error",
};
/* Platform Security Processor V2 */
static const char * const smca_psp2_mce_desc[] = {
	"High SRAM ECC or parity error",
	"Low SRAM ECC or parity error",
	"Instruction Cache Bank 0 ECC or parity error",
	"Instruction Cache Bank 1 ECC or parity error",
	"Instruction Tag Ram 0 parity error",
	"Instruction Tag Ram 1 parity error",
	"Data Cache Bank 0 ECC or parity error",
	"Data Cache Bank 1 ECC or parity error",
	"Data Cache Bank 2 ECC or parity error",
	"Data Cache Bank 3 ECC or parity error",
	"Data Tag Bank 0 parity error",
	"Data Tag Bank 1 parity error",
	"Data Tag Bank 2 parity error",
	"Data Tag Bank 3 parity error",
	"Dirty Data Ram parity error",
	"TLB Bank 0 parity error",
	"TLB Bank 1 parity error",
	"System Hub Read Buffer ECC or parity error",
};
/* System Management Unit */
static const char * const smca_smu_mce_desc[] = {
	"SMU RAM ECC or parity error",
};
/* System Management Unit V2 */
static const char * const smca_smu2_mce_desc[] = {
	"High SRAM ECC or parity error",
	"Low SRAM ECC or parity error",
	"Data Cache Bank A ECC or parity error",
	"Data Cache Bank B ECC or parity error",
	"Data Tag Cache Bank A ECC or parity error",
	"Data Tag Cache Bank B ECC or parity error",
	"Instruction Cache Bank A ECC or parity error",
	"Instruction Cache Bank B ECC or parity error",
	"Instruction Tag Cache Bank A ECC or parity error",
	"Instruction Tag Cache Bank B ECC or parity error",
	"System Hub Read Buffer ECC or parity error",
};
/* Microprocessor 5 Unit */
static const char * const smca_mp5_mce_desc[] = {
	"High SRAM ECC or parity error",
	"Low SRAM ECC or parity error",
	"Data Cache Bank A ECC or parity error",
	"Data Cache Bank B ECC or parity error",
	"Data Tag Cache Bank A ECC or parity error",
	"Data Tag Cache Bank B ECC or parity error",
	"Instruction Cache Bank A ECC or parity error",
	"Instruction Cache Bank B ECC or parity error",
	"Instruction Tag Cache Bank A ECC or parity error",
	"Instruction Tag Cache Bank B ECC or parity error",
};
/* Northbridge IO Unit */
static const char * const smca_nbio_mce_desc[] = {
	"ECC or Parity error",
	"PCIE error",
	"SDP ErrEvent error",
	"SDP Egress Poison Error",
	"IOHC Internal Poison Error",
};
/* PCI Express Unit */
static const char * const smca_pcie_mce_desc[] = {
	"CCIX PER Message logging",
	"CCIX Read Response with Status: Non-Data Error",
	"CCIX Write Response with Status: Non-Data Error",
	"CCIX Read Response with Status: Data Error",
	"CCIX Non-okay write response with data error",
};

static const char * const smca_pcie2_mce_desc[] = {
	"SDP Parity Error logging",
};

static const char * const smca_xgmipcs_mce_desc[] = {
	"Data Loss Error",
	"Training Error",
	"Flow Control Acknowledge Error",
	"Rx Fifo Underflow Error",
	"Rx Fifo Overflow Error",
	"CRC Error",
	"BER Exceeded Error",
	"Tx Vcid Data Error",
	"Replay Buffer Parity Error",
	"Data Parity Error",
	"Replay Fifo Overflow Error",
	"Replay Fifo Underflow Error",
	"Elastic Fifo Overflow Error",
	"Deskew Error",
	"Flow Control CRC Error",
	"Data Startup Limit Error",
	"FC Init Timeout Error",
	"Recovery Timeout Error",
	"Ready Serial Timeout Error",
	"Ready Serial Attempt Error",
	"Recovery Attempt Error",
	"Recovery Relock Attempt Error",
	"Replay Attempt Error",
	"Sync Header Error",
	"Tx Replay Timeout Error",
	"Rx Replay Timeout Error",
	"LinkSub Tx Timeout Error",
	"LinkSub Rx Timeout Error",
	"Rx CMD Pocket Error",
};

static const char * const smca_xgmiphy_mce_desc[] = {
	"RAM ECC Error",
	"ARC instruction buffer parity error",
	"ARC data buffer parity error",
	"PHY APB error",
};

static const char * const smca_waflphy_mce_desc[] = {
	"RAM ECC Error",
	"ARC instruction buffer parity error",
	"ARC data buffer parity error",
	"PHY APB error",
};

struct smca_mce_desc {
	const char * const *descs;
	unsigned int num_descs;
};

static struct smca_mce_desc smca_mce_descs[] = {
	[SMCA_LS]       = { smca_ls_mce_desc,   ARRAY_SIZE(smca_ls_mce_desc)  },
	[SMCA_LS_V2]	= { smca_ls2_mce_desc,	ARRAY_SIZE(smca_ls2_mce_desc) },
	[SMCA_IF]       = { smca_if_mce_desc,   ARRAY_SIZE(smca_if_mce_desc)  },
	[SMCA_L2_CACHE] = { smca_l2_mce_desc,   ARRAY_SIZE(smca_l2_mce_desc)  },
	[SMCA_DE]       = { smca_de_mce_desc,   ARRAY_SIZE(smca_de_mce_desc)  },
	[SMCA_EX]       = { smca_ex_mce_desc,   ARRAY_SIZE(smca_ex_mce_desc)  },
	[SMCA_FP]       = { smca_fp_mce_desc,   ARRAY_SIZE(smca_fp_mce_desc)  },
	[SMCA_L3_CACHE] = { smca_l3_mce_desc,   ARRAY_SIZE(smca_l3_mce_desc)  },
	[SMCA_CS]       = { smca_cs_mce_desc,   ARRAY_SIZE(smca_cs_mce_desc)  },
	[SMCA_CS_V2]    = { smca_cs2_mce_desc,  ARRAY_SIZE(smca_cs2_mce_desc) },
	[SMCA_PIE]      = { smca_pie_mce_desc,  ARRAY_SIZE(smca_pie_mce_desc) },
	[SMCA_UMC]      = { smca_umc_mce_desc,  ARRAY_SIZE(smca_umc_mce_desc) },
	[SMCA_UMC_V2]	= { smca_umc2_mce_desc,	ARRAY_SIZE(smca_umc2_mce_desc)	},
	[SMCA_PB]       = { smca_pb_mce_desc,   ARRAY_SIZE(smca_pb_mce_desc)  },
	[SMCA_PSP]      = { smca_psp_mce_desc,  ARRAY_SIZE(smca_psp_mce_desc) },
	[SMCA_PSP_V2]   = { smca_psp2_mce_desc, ARRAY_SIZE(smca_psp2_mce_desc)},
	[SMCA_SMU]      = { smca_smu_mce_desc,  ARRAY_SIZE(smca_smu_mce_desc) },
	[SMCA_SMU_V2]   = { smca_smu2_mce_desc, ARRAY_SIZE(smca_smu2_mce_desc)},
	[SMCA_MP5]      = { smca_mp5_mce_desc,  ARRAY_SIZE(smca_mp5_mce_desc) },
	[SMCA_NBIO]     = { smca_nbio_mce_desc, ARRAY_SIZE(smca_nbio_mce_desc)},
	[SMCA_PCIE]     = { smca_pcie_mce_desc, ARRAY_SIZE(smca_pcie_mce_desc)},
	[SMCA_PCIE_V2]	= { smca_pcie2_mce_desc,   ARRAY_SIZE(smca_pcie2_mce_desc)	},
	[SMCA_XGMI_PCS]	= { smca_xgmipcs_mce_desc, ARRAY_SIZE(smca_xgmipcs_mce_desc)	},
	[SMCA_XGMI_PHY]	= { smca_xgmiphy_mce_desc, ARRAY_SIZE(smca_xgmiphy_mce_desc)	},
	[SMCA_WAFL_PHY]	= { smca_waflphy_mce_desc, ARRAY_SIZE(smca_waflphy_mce_desc)	},
};

struct smca_hwid {
	unsigned int bank_type; /* Use with smca_bank_types for easy indexing.*/
	uint32_t mcatype_hwid;  /* mcatype,hwid bit 63-32 in MCx_IPID Register*/
};

static struct smca_hwid smca_hwid_mcatypes[] = {
	/* { bank_type, mcatype_hwid } */

	/* ZN Core (HWID=0xB0) MCA types */
	{ SMCA_LS,       0x000000B0 },
	{ SMCA_LS_V2,    0x001000B0 },
	{ SMCA_IF,       0x000100B0 },
	{ SMCA_L2_CACHE, 0x000200B0 },
	{ SMCA_DE,       0x000300B0 },
	/* HWID 0xB0 MCATYPE 0x4 is Reserved */
	{ SMCA_EX,       0x000500B0 },
	{ SMCA_FP,       0x000600B0 },
	{ SMCA_L3_CACHE, 0x000700B0 },

	/* Data Fabric MCA types */
	{ SMCA_CS,       0x0000002E },
	{ SMCA_CS_V2,    0x0002002E },
	{ SMCA_PIE,      0x0001002E },

	/* Unified Memory Controller MCA type */
	{ SMCA_UMC,      0x00000096 },
	/* Heterogeneous systems may have both UMC and UMC_v2 types on the same node. */
	{ SMCA_UMC_V2,   0x00010096 },

	/* Parameter Block MCA type */
	{ SMCA_PB,       0x00000005 },

	/* Platform Security Processor MCA type */
	{ SMCA_PSP,      0x000000FF },
	{ SMCA_PSP_V2,   0x000100FF },

	/* System Management Unit MCA type */
	{ SMCA_SMU,      0x00000001 },
	{ SMCA_SMU_V2,   0x00010001 },

	/* Microprocessor 5 Unit MCA type */
	{ SMCA_MP5,      0x00020001 },

	/* Northbridge IO Unit MCA type */
	{ SMCA_NBIO,     0x00000018 },

	/* PCI Express Unit MCA type */
	{ SMCA_PCIE,     0x00000046 },
	{ SMCA_PCIE_V2,  0x00010046 },

	/* Ext Global Memory Interconnect PCS MCA type */
	{ SMCA_XGMI_PCS, 0x00000050 },

	/* Ext Global Memory Interconnect PHY MCA type */
	{ SMCA_XGMI_PHY, 0x00000259 },

	/* WAFL PHY MCA type */
	{ SMCA_WAFL_PHY, 0x00000267 },
};

struct smca_bank_name {
	const char *name;
};

static struct smca_bank_name smca_names[] = {
	[SMCA_LS ... SMCA_LS_V2]	= { "Load Store Unit" },
	[SMCA_IF]			= { "Instruction Fetch Unit" },
	[SMCA_L2_CACHE]			= { "L2 Cache" },
	[SMCA_DE]			= { "Decode Unit" },
	[SMCA_RESERVED]			= { "Reserved" },
	[SMCA_EX]			= { "Execution Unit" },
	[SMCA_FP]			= { "Floating Point Unit" },
	[SMCA_L3_CACHE]			= { "L3 Cache" },
	[SMCA_CS ... SMCA_CS_V2]	= { "Coherent Slave" },
	[SMCA_PIE]			= { "Power, Interrupts, etc." },
	[SMCA_UMC]			= { "Unified Memory Controller" },
	[SMCA_UMC_V2]			= { "Unified Memory Controller V2" },
	[SMCA_PB]			= { "Parameter Block" },
	[SMCA_PSP ... SMCA_PSP_V2]	= { "Platform Security Processor" },
	[SMCA_SMU ... SMCA_SMU_V2]	= { "System Management Unit" },
	[SMCA_MP5]			= { "Microprocessor 5 Unit" },
	[SMCA_NBIO]			= { "Northbridge IO Unit" },
	[SMCA_PCIE ... SMCA_PCIE_V2]	= { "PCI Express Unit" },
	[SMCA_XGMI_PCS]			= { "Ext Global Memory Interconnect PCS Unit" },
	[SMCA_XGMI_PHY]			= { "Ext Global Memory Interconnect PHY Unit" },
	[SMCA_WAFL_PHY]			= { "WAFL PHY Unit" },

};

static void amd_decode_errcode(struct mce_event *e)
{

	decode_amd_errcode(e);

	if (e->status & MCI_STATUS_POISON)
		mce_snprintf(e->mcistatus_msg, "Poison consumed");

	if (e->status & MCI_STATUS_TCC)
		mce_snprintf(e->mcistatus_msg, "Task_context_corrupt");

}
/*
 * To find the UMC channel represented by this bank we need to match on its
 * instance_id. The instance_id of a bank is held in the lower 32 bits of its
 * IPID.
 */
static int find_umc_channel(struct mce_event *e)
{
	return EXTRACT(e->ipid, 0, 31) >> 20;
}

/*
 * The HBM memory managed by the UMCCH of the noncpu node
 * can be calculated based on the [15:12]bits of IPID
 */
static int find_hbm_channel(struct mce_event *e)
{
	int umc, tmp;

	umc = EXTRACT(e->ipid, 0, 31) >> 20;

	/*
	 * The HBM channel managed by the UMC of the noncpu node
	 * can be calculated based on the [15:12]bits of IPID as follows
	 */
	tmp = ((e->ipid >> 12) & 0xf);

	return (umc % 2) ? tmp + 4 : tmp;
}

/* Decode extended errors according to Scalable MCA specification */
static void decode_smca_error(struct mce_event *e)
{
	enum smca_bank_types bank_type;
	const char *ip_name;
	unsigned short xec = (e->status >> 16) & 0x3f;
	const struct smca_hwid *s_hwid;
	uint32_t mcatype_hwid = EXTRACT(e->ipid, 32, 63);
	uint8_t mcatype_instancehi = EXTRACT(e->ipid, 44, 47);
	unsigned int csrow = -1, channel = -1;
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(smca_hwid_mcatypes); i++) {
		s_hwid = &smca_hwid_mcatypes[i];
		if (mcatype_hwid == s_hwid->mcatype_hwid) {
			bank_type = s_hwid->bank_type;
			break;
		}
		if (mcatype_instancehi >= NONCPU_NODE_INDEX)
			bank_type = SMCA_UMC_V2;
	}

	if (i >= MAX_NR_BANKS) {
		strcpy(e->mcastatus_msg, "Couldn't find bank type with IPID");
		return;
	}

	if (bank_type >= N_SMCA_BANK_TYPES) {
		strcpy(e->mcastatus_msg, "Don't know how to decode this bank");
		return;
	}

	if (bank_type == SMCA_RESERVED) {
		strcpy(e->mcastatus_msg, "Bank 4 is reserved.\n");
		return;
	}

	ip_name = smca_names[bank_type].name;

	mce_snprintf(e->bank_name, "%s (bank=%d)", ip_name, e->bank);

	/* Only print the descriptor of valid extended error code */
	if (xec < smca_mce_descs[bank_type].num_descs)
		mce_snprintf(e->mcastatus_msg,
			     " %s.\n", smca_mce_descs[bank_type].descs[xec]);

	if (bank_type == SMCA_UMC && xec == 0) {
		channel = find_umc_channel(e);
		csrow = e->synd & 0x7; /* Bit 0, 1 ,2 */
		mce_snprintf(e->mc_location, "memory_channel=%d,csrow=%d",
			     channel, csrow);
	}

	if (bank_type == SMCA_UMC_V2 && xec == 0) {
		/* The UMCPHY is reported as csrow in case of noncpu nodes */
		csrow = find_umc_channel(e) / 2;
		/* UMCCH is managing the HBM memory */
		channel = find_hbm_channel(e);
		mce_snprintf(e->mc_location, "memory_channel=%d,csrow=%d",
			     channel, csrow);
	}

}

int parse_amd_smca_event(struct ras_events *ras, struct mce_event *e)
{
	uint64_t mcgstatus = e->mcgstatus;

	mce_snprintf(e->mcgstatus_msg, "mcgstatus=%lld",
		    (long long)e->mcgstatus);

	if (mcgstatus & MCG_STATUS_RIPV)
		mce_snprintf(e->mcgstatus_msg, "RIPV");
	if (mcgstatus & MCG_STATUS_EIPV)
		mce_snprintf(e->mcgstatus_msg, "EIPV");
	if (mcgstatus & MCG_STATUS_MCIP)
		mce_snprintf(e->mcgstatus_msg, "MCIP");

	decode_smca_error(e);
	amd_decode_errcode(e);
	return 0;
}
