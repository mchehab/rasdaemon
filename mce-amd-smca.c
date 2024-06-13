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
	SMCA_CS_V2_QUIRK,
	SMCA_PIE,       /* Power, Interrupts, etc. */
	SMCA_UMC,       /* Unified Memory Controller */
	SMCA_UMC_QUIRK,
	SMCA_UMC_V2,
	SMCA_MA_LLC,	/* Memory Attached Last Level Cache */
	SMCA_PB,        /* Parameter Block */
	SMCA_PSP,       /* Platform Security Processor */
	SMCA_PSP_V2,
	SMCA_SMU,       /* System Management Unit */
	SMCA_SMU_V2,
	SMCA_MP5,	/* Microprocessor 5 Unit */
	SMCA_MPDMA,	/* MPDMA Unit */
	SMCA_NBIO,	/* Northbridge IO Unit */
	SMCA_PCIE,	/* PCI Express Unit */
	SMCA_PCIE_V2,
	SMCA_XGMI_PCS,	/* xGMI PCS Unit */
	SMCA_NBIF,	/* NBIF Unit */
	SMCA_SHUB,	/* System Hub Unit */
	SMCA_SATA,	/* SATA Unit */
	SMCA_USB,	/* USB Unit */
	SMCA_USR_DP,	/* Ultra Short Reach Data Plane Controller */
	SMCA_USR_CP,	/* Ultra Short Reach Control Plane Controller */
	SMCA_GMI_PCS,	/* GMI PCS Unit */
	SMCA_XGMI_PHY,	/* xGMI PHY Unit */
	SMCA_WAFL_PHY,	/* WAFL PHY Unit */
	SMCA_GMI_PHY,	/* GMI PHY Unit */
	N_SMCA_BANK_TYPES
};

/* Maximum number of MCA banks per CPU. */
#define MAX_NR_BANKS	64

#define MCI_IPID_MCATYPE    0xFFFF0000
#define MCI_IPID_HWID       0xFFF

/* Obtain HWID_MCATYPE Tuple on SMCA Systems */
#define HWID_MCATYPE(hwid, mcatype) (((hwid) << 16) | (mcatype))

/*
 * On Newer heterogeneous systems from AMD with CPU and GPU nodes connected
 * via xGMI links, the NON CPU Nodes are enumerated from index 8
 */
#define NONCPU_NODE_INDEX	8

/* SMCA Extended error strings */
static const char * const smca_ls_mce_desc[] = {
	"Load queue parity",
	"Store queue parity",
	"Miss address buffer payload parity",
	"L1 TLB parity",
	"DC Tag error type 5",
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
	"Error on SCB cacheline state or address field",
	"Error on SCB data, commit pipe 0",
	"Error on SCB data, commit pipe 1",
	"Error on SCB data for non-cacheable DRAM or IO",
	"System Read Data Error detected by write combine buffer",
	"Hardware Asserts",
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
	"A parity error was detected in an STLF, SCB EMEM entry, store data mask or SRB store data by any access",
};

static const char * const smca_if_mce_desc[] = {
	"microtag probe port parity error",
	"IC microtag or full tag multi-hit error",
	"IC full tag parity",
	"IC data array parity",
	"PRQ Parity Error",
	"L0 ITLB parity error",
	"L1-TLB parity error",
	"L2-TLB parity error",
	"BPQ snoop parity on Thread 0",
	"BPQ snoop parity on Thread 1",
	"BP L1-BTB Multi-Hit Error",
	"BP L2-BTB Multi-Hit Error",
	"L2 Cache Response Poison error",
	"System Read Data error",
	"Hardware Assertion Error",
	"L1-TLB Multi-Hit",
	"L2-TLB Multi-Hit",
	"BSR Parity Error",
	"CT MCE",
};

static const char * const smca_l2_mce_desc[] = {
	"L2M Tag Multiple-Way-Hit error",
	"L2M Tag or State Array ECC Error",
	"L2M Data Array ECC Error",
	"Hardware Assert Error",
	"SDP Read Response Parity Error",
	"Error initiated by programmable state machine",
};

static const char * const smca_de_mce_desc[] = {
	"Micro-op cache tag array parity error",
	"Micro-op cache data array parity error",
	"IBB Register File parity error",
	"Micro-op queue parity error",
	"Instruction dispatch queue parity error",
	"Fetch address FIFO parity error",
	"Patch RAM data parity error",
	"Patch RAM sequencer parity error",
	"Micro-op fetch queue parity error",
	"Hardware Assertion MCA Error",
};

static const char * const smca_ex_mce_desc[] = {
	"Watchdog timeout error",
	"Physical register file parity error",
	"Flag register file parity error",
	"Immediate displacement register file parity error",
	"Address generator payload parity error",
	"EX payload parity error",
	"Checkpoint queue parity error",
	"Retire dispatch queue parity error",
	"Retire status queue parity error",
	"Scheduler queue parity error",
	"Branch buffer queue parity error",
	"Hardware Assertion error",
	"Spec Map parity error",
	"Retire Map parity error",
};

static const char * const smca_fp_mce_desc[] = {
	"Physical register file (PRF) parity error",
	"Freelist (FL) parity error",
	"Schedule queue parity error",
	"NSQ parity error",
	"Retire queue (RQ) parity error",
	"Status register file (SRF) parity error",
	"Hardware assertion",
	"Physical K mask register file (KRF) parity error",
};

static const char * const smca_l3_mce_desc[] = {
	"Shadow tag macro ECC error",
	"Shadow tag macro multi-way-hit error",
	"L3M tag ECC error",
	"L3M tag multi-way-hit error",
	"L3M data ECC error",
	"SDP Parity Error from XI",
	"L3 victim queue Data Fabric error",
	"L3 Hardware Assertion",
	"XI WCB Parity Poison Creation event",
	"Machine check error initiated by DSM action",
};

static const char * const smca_cs_mce_desc[] = {
	"Illegal request",
	"Address violation",
	"Security violation",
	"Illegal response",
	"Unexpected response",
	"Request or Probe Parity Error",
	"Read Response Parity Error",
	"Atomic request parity error",
	"Probe Filter ECC Error",
};

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
	"Illegal Request on the no data channel",
	"Address Violation on the no data channel",
	"Security Violation on the no data channel",
	"Hardware Assert Error",
	"Shadow Tag Array Protocol Error",
	"Shadow Tag ECC Error",
	"Shadow Tag Transaction Error",
};

/*
 * Per Genoa's revision guide, erratum 1384, existing bit definitions
 * are reassigned for SMCA CS bank type.
 */
static const char * const smca_cs2_quirk_mce_desc[] = {
	"Illegal Request",
	"Address Violation",
	"Security Violation",
	"Illegal Response",
	"Unexpected Response",
	"Request or Probe Parity Error",
	"Read Response Parity Error",
	"Atomic Request Parity Error",
	"SDP read response had no match in the CS queue",
	"SDP read response had an unexpected RETRY error",
	"Counter overflow error",
	"Counter underflow error",
	"Probe Filter Protocol Error",
	"Probe Filter ECC Error",
	"Illegal Request on the no data channel",
	"Address Violation on the no data channel",
	"Security Violation on the no data channel",
	"Hardware Assert Error",
};

static const char * const smca_pie_mce_desc[] = {
	"Hardware assert",
	"Register security violation",
	"Link error",
	"Poison data consumption",
	"A deferred error was detected in the DF",
	"Watch Dog Timer",
	"An SRAM ECC error was detected in the CNLI block",
	"Register access during DF Cstate",
	"DSM Error",
};

static const char * const smca_umc_mce_desc[] = {
	"DRAM ECC error",
	"Data poison error on DRAM",
	"SDP parity error",
	"Advanced peripheral bus error",
	"Command/address parity error",
	"Write data CRC error",
	"DCQ SRAM ECC error",
	"AES SRAM ECC error",
	"ECS Row Error",
	"ECS Error",
	"UMC Throttling Error",
	"Read CRC Error",
	"Reserved",
	"Reserved",
	"Reserved",
	"Reserved",
	"RFM SRAM ECC error",
};

static const char * const smca_umc_quirk_mce_desc[] = {
	"DRAM On Die ECC error",
	"Data poison error",
	"SDP parity error",
	"Reserved",
	"Address/Command parity error",
	"HBM Write data parity error",
	"Consolidated SRAM ECC error",
	"Reserved",
	"Reserved",
	"Rdb SRAM ECC error",
	"Thermal throttling",
	"HBM Read Data Parity error",
	"Reserved",
	"UMC FW Error",
	"SRAM Parity Error",
	"HBM CRC Error",
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

static const char * const smca_mall_mce_desc[] = {
	"Counter overflow error",
	"Counter underflow error",
	"Write Data Parity Error",
	"Read Response Parity Error",
	"Cache Tag ECC Error Macro 0",
	"Cache Tag ECC Error Macro 1",
	"Cache Data ECC Error"
};

static const char * const smca_pb_mce_desc[] = {
	"An ECC error in the Parameter Block RAM array"
};

static const char * const smca_psp_mce_desc[] = {
	"An ECC or parity error in a PSP RAM instance",
};

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
	"FUSE IP SRAM ECC or parity error",
	"PCRU FUSE SRAM ECC or parity error",
	"SIB SRAM parity error",
	"mpASP SECEMC Error",
	"mpASP A5 Hang",
	"SIB WDT error",
};

static const char * const smca_smu_mce_desc[] = {
	"An ECC or parity error in an SMU RAM instance",
};

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
	"PHY RAS ECC Error",
	[12 ... 57] = "Reserved",
	"A correctable error from a GFX Sub-IP",
	"A fatal error from a GFX Sub-IP",
	"Reserved",
	"Reserved",
	"A poison error from a GFX Sub-IP",
	"Reserved",
};

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
	"Fuse SRAM ECC or parity error",
};

static const char * const smca_mpdma_mce_desc[] = {
	"Main SRAM [31:0] bank ECC or parity error",
	"Main SRAM [63:32] bank ECC or parity error",
	"Main SRAM [95:64] bank ECC or parity error",
	"Main SRAM [127:96] bank ECC or parity error",
	"Data Cache Bank A ECC or parity error",
	"Data Cache Bank B ECC or parity error",
	"Data Tag Cache Bank A ECC or parity error",
	"Data Tag Cache Bank B ECC or parity error",
	"Instruction Cache Bank A ECC or parity error",
	"Instruction Cache Bank B ECC or parity error",
	"Instruction Tag Cache Bank A ECC or parity error",
	"Instruction Tag Cache Bank B ECC or parity error",
	"Data Cache Bank A ECC or parity error",
	"Data Cache Bank B ECC or parity error",
	"Data Tag Cache Bank A ECC or parity error",
	"Data Tag Cache Bank B ECC or parity error",
	"Instruction Cache Bank A ECC or parity error",
	"Instruction Cache Bank B ECC or parity error",
	"Instruction Tag Cache Bank A ECC or parity error",
	"Instruction Tag Cache Bank B ECC or parity error",
	"Data Cache Bank A ECC or parity error",
	"Data Cache Bank B ECC or parity error",
	"Data Tag Cache Bank A ECC or parity error",
	"Data Tag Cache Bank B ECC or parity error",
	"Instruction Cache Bank A ECC or parity error",
	"Instruction Cache Bank B ECC or parity error",
	"Instruction Tag Cache Bank A ECC or parity error",
	"Instruction Tag Cache Bank B ECC or parity error",
	"System Hub Read Buffer ECC or parity error",
	"MPDMA TVF DVSEC Memory ECC or parity error",
	"MPDMA TVF MMIO Mailbox0 ECC or parity error",
	"MPDMA TVF MMIO Mailbox1 ECC or parity error",
	"MPDMA TVF Doorbell Memory ECC or parity error",
	"MPDMA TVF SDP Slave Memory 0 ECC or parity error",
	"MPDMA TVF SDP Slave Memory 1 ECC or parity error",
	"MPDMA TVF SDP Slave Memory 2 ECC or parity error",
	"MPDMA TVF SDP Master Memory 0 ECC or parity error",
	"MPDMA TVF SDP Master Memory 1 ECC or parity error",
	"MPDMA TVF SDP Master Memory 2 ECC or parity error",
	"MPDMA TVF SDP Master Memory 3 ECC or parity error",
	"MPDMA TVF SDP Master Memory 4 ECC or parity error",
	"MPDMA TVF SDP Master Memory 5 ECC or parity error",
	"MPDMA TVF SDP Master Memory 6 ECC or parity error",
	"SDP Watchdog Timer expired",
	"MPDMA PTE Command FIFO ECC or parity error",
	"MPDMA PTE Hub Data FIFO ECC or parity error",
	"MPDMA PTE Internal Data FIFO ECC or parity error",
	"MPDMA PTE Command Memory DMA ECC or parity error",
	"MPDMA PTE Command Memory Internal ECC or parity error",
	"MPDMA TVF SDP Master Memory 7 ECC or parity error",
};

static const char * const smca_nbio_mce_desc[] = {
	"ECC or Parity error",
	"PCIE error",
	"External SDP ErrEvent error",
	"SDP Egress Poison error",
	"Internal Poison error",
	"Internal system fatal error event",
};

static const char * const smca_pcie_mce_desc[] = {
	"CCIX PER Message logging",
	"CCIX Read Response with Status: Non-Data Error",
	"CCIX Write Response with Status: Non-Data Error",
	"CCIX Read Response with Status: Data Error",
	"CCIX Non-okay write response with data error",
};

static const char * const smca_pcie2_mce_desc[] = {
	"SDP Data Parity Error logging",
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

static const char * const smca_nbif_mce_desc[] = {
	"Timeout error from GMI",
	"SRAM ECC error",
	"NTB Error Event",
	"SDP Parity error",
};

static const char * const smca_sata_mce_desc[] = {
	"Parity error for port 0",
	"Parity error for port 1",
	"Parity error for port 2",
	"Parity error for port 3",
	"Parity error for port 4",
	"Parity error for port 5",
	"Parity error for port 6",
	"Parity error for port 7",
};

static const char * const smca_usb_mce_desc[] = {
	"Parity error or ECC error for S0 RAM0",
	"Parity error or ECC error for S0 RAM1",
	"Parity error or ECC error for S0 RAM2",
	"Parity error for PHY RAM0",
	"Parity error for PHY RAM1",
	"AXI Slave Response error",
};

static const char * const smca_usrdp_mce_desc[] = {
	"Mst CMD Error",
	"Mst Rx FIFO Error",
	"Mst Deskew Error",
	"Mst Detect Timeout Error",
	"Mst FlowControl Error",
	"Mst DataValid FIFO Error",
	"Mac LinkState Error",
	"Deskew Error",
	"Init Timeout Error",
	"Init Attempt Error",
	"Recovery Timeout Error",
	"Recovery Attempt Error",
	"Eye Training Timeout Error",
	"Data Startup Limit Error",
	"LS0 Exit Error",
	"PLL powerState Update Timeout Error",
	"Rx FIFO Error",
	"Lcu Error",
	"Conv CECC Error",
	"Conv UECC Error",
	"Reserved",
	"Rx DataLoss Error",
	"Replay CECC Error",
	"Replay UECC Error",
	"CRC Error",
	"BER Exceeded Error",
	"FC Init Timeout Error",
	"FC Init Attempt Error",
	"Replay Timeout Error",
	"Replay Attempt Error",
	"Replay Underflow Error",
	"Replay Overflow Error",
};

static const char * const smca_usrcp_mce_desc[] = {
	"Packet Type Error",
	"Rx FIFO Error",
	"Deskew Error",
	"Rx Detect Timeout Error",
	"Data Parity Error",
	"Data Loss Error",
	"Lcu Error",
	"HB1 Handshake Timeout Error",
	"HB2 Handshake Timeout Error",
	"Clk Sleep Rsp Timeout Error",
	"Clk Wake Rsp Timeout Error",
	"Reset Attack Error",
	"Remote Link Fatal Error",
};

static const char * const smca_gmipcs_mce_desc[] = {
	"Data Loss Error",
	"Training Error",
	"Replay Parity Error",
	"Rx Fifo Underflow Error",
	"Rx Fifo Overflow Error",
	"CRC Error",
	"BER Exceeded Error",
	"Tx Fifo Underflow Error",
	"Replay Buffer Parity Error",
	"Tx Overflow Error",
	"Replay Fifo Overflow Error",
	"Replay Fifo Underflow Error",
	"Elastic Fifo Overflow Error",
	"Deskew Error",
	"Offline Error",
	"Data Startup Limit Error",
	"FC Init Timeout Error",
	"Recovery Timeout Error",
	"Ready Serial Timeout Error",
	"Ready Serial Attempt Error",
	"Recovery Attempt Error",
	"Recovery Relock Attempt Error",
	"Deskew Abort Error",
	"Rx Buffer Error",
	"Rx LFDS Fifo Overflow Error",
	"Rx LFDS Fifo Underflow Error",
	"LinkSub Tx Timeout Error",
	"LinkSub Rx Timeout Error",
	"Rx CMD Packet Error",
	"LFDS Training Timeout Error",
	"LFDS FC Init Timeout Error",
	"Data Loss Error",
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
	[SMCA_CS_V2_QUIRK] = { smca_cs2_quirk_mce_desc, ARRAY_SIZE(smca_cs2_quirk_mce_desc)},
	[SMCA_PIE]      = { smca_pie_mce_desc,  ARRAY_SIZE(smca_pie_mce_desc) },
	[SMCA_UMC]      = { smca_umc_mce_desc,  ARRAY_SIZE(smca_umc_mce_desc) },
	[SMCA_UMC_QUIRK] = { smca_umc_quirk_mce_desc,  ARRAY_SIZE(smca_umc_quirk_mce_desc) },
	[SMCA_UMC_V2]	= { smca_umc2_mce_desc,	ARRAY_SIZE(smca_umc2_mce_desc)	},
	[SMCA_MA_LLC]	= { smca_mall_mce_desc, ARRAY_SIZE(smca_mall_mce_desc)	},
	[SMCA_PB]       = { smca_pb_mce_desc,   ARRAY_SIZE(smca_pb_mce_desc)  },
	[SMCA_PSP]      = { smca_psp_mce_desc,  ARRAY_SIZE(smca_psp_mce_desc) },
	[SMCA_PSP_V2]   = { smca_psp2_mce_desc, ARRAY_SIZE(smca_psp2_mce_desc)},
	[SMCA_SMU]      = { smca_smu_mce_desc,  ARRAY_SIZE(smca_smu_mce_desc) },
	[SMCA_SMU_V2]   = { smca_smu2_mce_desc, ARRAY_SIZE(smca_smu2_mce_desc)},
	[SMCA_MP5]      = { smca_mp5_mce_desc,  ARRAY_SIZE(smca_mp5_mce_desc) },
	[SMCA_MPDMA]    = { smca_mpdma_mce_desc,    ARRAY_SIZE(smca_mpdma_mce_desc) },
	[SMCA_NBIO]     = { smca_nbio_mce_desc, ARRAY_SIZE(smca_nbio_mce_desc)},
	[SMCA_PCIE]     = { smca_pcie_mce_desc, ARRAY_SIZE(smca_pcie_mce_desc)},
	[SMCA_PCIE_V2]	= { smca_pcie2_mce_desc,   ARRAY_SIZE(smca_pcie2_mce_desc)	},
	[SMCA_XGMI_PCS]	= { smca_xgmipcs_mce_desc, ARRAY_SIZE(smca_xgmipcs_mce_desc)	},
	/* NBIF and SHUB have the same error descriptions, for now. */
	[SMCA_NBIF] = { smca_nbif_mce_desc, ARRAY_SIZE(smca_nbif_mce_desc)  },
	[SMCA_SHUB] = { smca_nbif_mce_desc, ARRAY_SIZE(smca_nbif_mce_desc)  },
	[SMCA_SATA] = { smca_sata_mce_desc, ARRAY_SIZE(smca_sata_mce_desc)  },
	[SMCA_USB]  = { smca_usb_mce_desc,  ARRAY_SIZE(smca_usb_mce_desc)   },
	[SMCA_USR_DP]	= { smca_usrdp_mce_desc,  ARRAY_SIZE(smca_usrdp_mce_desc)   },
	[SMCA_USR_CP]	= { smca_usrcp_mce_desc,  ARRAY_SIZE(smca_usrcp_mce_desc)   },
	[SMCA_GMI_PCS]  = { smca_gmipcs_mce_desc,  ARRAY_SIZE(smca_gmipcs_mce_desc) },
	/* All the PHY bank types have the same error descriptions, for now. */
	[SMCA_XGMI_PHY]	= { smca_xgmiphy_mce_desc, ARRAY_SIZE(smca_xgmiphy_mce_desc)	},
	[SMCA_WAFL_PHY]	= { smca_xgmiphy_mce_desc, ARRAY_SIZE(smca_xgmiphy_mce_desc)	},
	[SMCA_GMI_PHY]  = { smca_xgmiphy_mce_desc, ARRAY_SIZE(smca_xgmiphy_mce_desc)    },
};

struct smca_hwid {
	enum smca_bank_types bank_type;
	uint32_t mcatype_hwid;  /* mcatype,hwid bit 63-32 in MCx_IPID Register*/
};

static struct smca_hwid smca_hwid_mcatypes[] = {
	/* { bank_type, mcatype_hwid } */

	/* ZN Core (HWID=0xB0) MCA types */
	{ SMCA_LS,	 HWID_MCATYPE(0xB0, 0x0) },
	{ SMCA_LS_V2,	 HWID_MCATYPE(0xB0, 0x10) },
	{ SMCA_IF,	 HWID_MCATYPE(0xB0, 0x1) },
	{ SMCA_L2_CACHE,	 HWID_MCATYPE(0xB0, 0x2) },
	{ SMCA_DE,	 HWID_MCATYPE(0xB0, 0x3) },
	/* HWID 0xB0 MCATYPE 0x4 is Reserved */
	{ SMCA_EX,	 HWID_MCATYPE(0xB0, 0x5) },
	{ SMCA_FP,	 HWID_MCATYPE(0xB0, 0x6) },
	{ SMCA_L3_CACHE,	 HWID_MCATYPE(0xB0, 0x7) },

	/* Data Fabric MCA types */
	{ SMCA_CS,	 HWID_MCATYPE(0x2E, 0x0) },
	{ SMCA_PIE,	 HWID_MCATYPE(0x2E, 0x1) },
	{ SMCA_CS_V2,	 HWID_MCATYPE(0x2E, 0x2) },
	{ SMCA_CS_V2_QUIRK,	 HWID_MCATYPE(0x0, 0x1) },

	/* Unified Memory Controller MCA type */
	{ SMCA_UMC,	 HWID_MCATYPE(0x96, 0x0) },
	{ SMCA_UMC_QUIRK,	 HWID_MCATYPE(0x0, 0x2) },
	/* Heterogeneous systems may have both UMC and UMC_v2 types on the same node. */
	{ SMCA_UMC_V2,	 HWID_MCATYPE(0x96, 0x1) },
	/* Memory Attached Last Level Cache */
	{ SMCA_MA_LLC,	 HWID_MCATYPE(0x2E, 0x4) },

	/* Parameter Block MCA type */
	{ SMCA_PB,	 HWID_MCATYPE(0x05, 0x0) },

	/* Platform Security Processor MCA type */
	{ SMCA_PSP,	 HWID_MCATYPE(0xFF, 0x0) },
	{ SMCA_PSP_V2,	 HWID_MCATYPE(0xFF, 0x1) },

	/* System Management Unit MCA type */
	{ SMCA_SMU,	 HWID_MCATYPE(0x01, 0x0) },
	{ SMCA_SMU_V2,	 HWID_MCATYPE(0x01, 0x1) },

	/* Microprocessor 5 Unit MCA type */
	{ SMCA_MP5,	 HWID_MCATYPE(0x01, 0x2) },

	/* MPDMA MCA Type */
	{ SMCA_MPDMA,	 HWID_MCATYPE(0x01, 0x3) },

	/* Northbridge IO Unit MCA type */
	{ SMCA_NBIO,	 HWID_MCATYPE(0x18, 0x0) },

	/* PCI Express Unit MCA type */
	{ SMCA_PCIE,	 HWID_MCATYPE(0x46, 0x0) },
	{ SMCA_PCIE_V2,	 HWID_MCATYPE(0x46, 0x1) },

	/* Ext Global Memory Interconnect PCS MCA type */
	{ SMCA_XGMI_PCS,	 HWID_MCATYPE(0x50, 0x0) },

	{ SMCA_NBIF,	 HWID_MCATYPE(0x6C, 0x0) },

	{ SMCA_SHUB,	 HWID_MCATYPE(0x80, 0x0) },
	{ SMCA_SATA,	 HWID_MCATYPE(0xA8, 0x0) },
	{ SMCA_USB,	 HWID_MCATYPE(0xAA, 0x0) },

	/* Ultra Short Reach Data and Control Plane Controller */
	{ SMCA_USR_DP,	 HWID_MCATYPE(0x170, 0x0) },
	{ SMCA_USR_CP,	 HWID_MCATYPE(0x180, 0x0) },

	{ SMCA_GMI_PCS,	 HWID_MCATYPE(0x241, 0x0) },

	/* Ext Global Memory Interconnect PHY MCA type */
	{ SMCA_XGMI_PHY,	 HWID_MCATYPE(0x259, 0x0) },

	/* WAFL PHY MCA type */
	{ SMCA_WAFL_PHY,	 HWID_MCATYPE(0x267, 0x0) },

	{ SMCA_GMI_PHY,	 HWID_MCATYPE(0x269, 0x0) },
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
	[SMCA_CS ... SMCA_CS_V2_QUIRK]	= { "Coherent Slave" },
	[SMCA_PIE]			= { "Power, Interrupts, etc." },
	[SMCA_UMC ... SMCA_UMC_QUIRK]	= { "Unified Memory Controller" },
	[SMCA_UMC_V2]			= { "Unified Memory Controller V2" },
	[SMCA_MA_LLC]			= { "Memory Attached Last Level Cache" },
	[SMCA_PB]			= { "Parameter Block" },
	[SMCA_PSP ... SMCA_PSP_V2]	= { "Platform Security Processor" },
	[SMCA_SMU ... SMCA_SMU_V2]	= { "System Management Unit" },
	[SMCA_MP5]			= { "Microprocessor 5 Unit" },
	[SMCA_MPDMA]			= { "MPDMA Unit" },
	[SMCA_NBIO]			= { "Northbridge IO Unit" },
	[SMCA_PCIE ... SMCA_PCIE_V2]	= { "PCI Express Unit" },
	[SMCA_XGMI_PCS]			= { "Ext Global Memory Interconnect PCS Unit" },
	[SMCA_NBIF]			= { "NBIF Unit" },
	[SMCA_SHUB]			= { "System Hub Unit" },
	[SMCA_SATA]			= { "SATA Unit" },
	[SMCA_USB]			= { "USB Unit" },
	[SMCA_USR_DP]			= { "Ultra Short Reach Data Plane Controller" },
	[SMCA_USR_CP]			= { "Ultra Short Reach Control Plane Controller" },
	[SMCA_GMI_PCS]			= { "Global Memory Interconnect PCS Unit" },
	[SMCA_XGMI_PHY]			= { "Ext Global Memory Interconnect PHY Unit" },
	[SMCA_WAFL_PHY]			= { "WAFL PHY Unit" },
	[SMCA_GMI_PHY]			= { "Global Memory Interconnect PHY Unit" },
};

void amd_decode_errcode(struct mce_event *e)
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

static inline void fixup_hwid(struct mce_priv *m, uint32_t *hwid_mcatype)
{
	if (m->family == 0x19) {
		switch (m->model) {
		/*
		 * Per Genoa's revision guide, erratum 1384, some SMCA Extended
		 * Error Codes and SMCA Control bits are incorrect for SMCA CS
		 * bank type.
		 */
		case 0x10 ... 0x1F:
		case 0x60 ... 0x7B:
		case 0xA0 ... 0xAF:
			if (*hwid_mcatype == HWID_MCATYPE(0x2E, 0x2))
				*hwid_mcatype = HWID_MCATYPE(0x0, 0x1);
			break;
		case 0x90 ... 0x9F:
			if (*hwid_mcatype == HWID_MCATYPE(0x96, 0x0))
				*hwid_mcatype = HWID_MCATYPE(0x0, 0x2);
			break;
		default:
			break;
		}
	} else if (m->family == 0x1A) {
		switch (m->model) {
		case 0x40 ... 0x4F:
			if (*hwid_mcatype == HWID_MCATYPE(0x2E, 0x2))
				*hwid_mcatype = HWID_MCATYPE(0x0, 0x1);
			break;
		default:
			break;
		}
	}
}

/* Decode extended errors according to Scalable MCA specification */
void decode_smca_error(struct mce_event *e, struct mce_priv *m)
{
	enum smca_bank_types bank_type;
	const char *ip_name;
	uint32_t mcatype_hwid = 0;
	unsigned short xec = (e->status >> 16) & 0x3f;
	const struct smca_hwid *s_hwid;
	uint32_t ipid_high = EXTRACT(e->ipid, 32, 63);
	uint8_t mcatype_instancehi = EXTRACT(e->ipid, 44, 47);
	unsigned int csrow = -1, channel = -1;
	unsigned int i;

	mcatype_hwid = HWID_MCATYPE(ipid_high & MCI_IPID_HWID,
				    (ipid_high & MCI_IPID_MCATYPE) >> 16);

	fixup_hwid(m, &mcatype_hwid);

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
			     "%s. Ext Err Code: %d",
			     smca_mce_descs[bank_type].descs[xec],
			     xec);

	if ((bank_type == SMCA_UMC || bank_type == SMCA_UMC_QUIRK) && xec == 0) {
		if ((m->family == 0x19) && (m->model >= 0x90 && m->model <= 0x9f)) {
			/* MCA_IPID[InstanceIdHi] give the AMD Node Die ID */
			mce_snprintf(e->mc_location, "memory_die_id=%d", mcatype_instancehi / 4);
		} else {
			channel = find_umc_channel(e);
			csrow = e->synd & 0x7; /* Bit 0, 1 ,2 */
			mce_snprintf(e->mc_location, "memory_channel=%d,csrow=%d",
				     channel, csrow);
		}
	}

	if (bank_type == SMCA_UMC_V2 && xec == 0) {
		/* The UMCPHY is reported as csrow in case of noncpu nodes */
		csrow = find_umc_channel(e) / 2;
		/* UMCCH is managing the HBM memory */
		channel = find_hbm_channel(e);
		mce_snprintf(e->mc_location, "memory_channel=%d,csrow=%d",
			     channel, csrow);
	}

	if (e->vdata_len) {
		uint64_t smca_config = e->vdata[2];

		/*
		 * BIT 9 of the CONFIG register of a few SMCA Bank types indicates
		 * presence of FRU Text in SYND 1 / 2 registers
		 */
		if (smca_config & BIT(9))
			memcpy(e->frutext, e->vdata, 16);
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

	decode_smca_error(e, ras->mce_priv);
	amd_decode_errcode(e);
	return 0;
}
