// SPDX-License-Identifier: GPL-2.0

/*
 * The code below came from Qiuxu Zhuo's mcelog code,
 * released under GNU Public General License, v2
 *
 * Copyright (C) 2025 Intel Corporation
 * Decode Intel Diamond Rapids specific machine check errors.
 */

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "bitfield.h"
#include "ras-mce-handler.h"

static char *cbb_punit_0[] = {
	[0x01] = "CR4_MCE_CLEAR: MCE when CR4.MCE is clear",
	[0x02] = "MCE_MCIP_SET: MCE when MCIP bit is set",
	[0x03] = "MCE_UNDER_WFS: MCE under WPS",
	[0x04] = "MCE_LT_HANDSHAKE: Unrecoverable error during security flow execution",
	[0x05] = "TRIPLE_FAULT: SW triple fault shutdown",
	[0x06] = "VMX_ABORT: VMX exit consistency check failures",
	[0x07] = "RSM_CONSISTENCY_FAIL: RSM consistency check failures",
	[0x08] = "SMM_PROTECTED_ENTRY_FAIL: Invalid conditions on protected mode SMM entry",
	[0x09] = "UCODE_PATCH_LOAD_FAIL: Unrecoverable error during security flow execution",
};

static char *cbb_punit_8[] = {
	[0x16] = "MCA_DVFS_COUNTER_EXPIRED",
	[0x23] = "MCA_UNSUPP_RESP",
	[0x25] = "MCA_FUSA_MBIST",
	[0x26] = "MCA_SA_PLL_LOCKED",
	[0x27] = "MCA_XTAL_FREQ_MON_ERROR",
	[0x28] = "MCA_PMSB_FUSE_PULLER_ERROR",
	[0x29] = "MCA_GPSB_FUSE_PULLER_ERROR",
	[0x30] = "MCA_PCODE_WATCHDOG",
	[0x31] = "MCA_BGR_FUSA_ERR",
	[0x32] = "MCA_CORE_FIVR_ERR",
	[0x33] = "MCA_D2D_FIVR_ERR",
	[0x34] = "MCA_MLC_FIVR_ERR",
	[0x35] = "MCA_RING_FIVR_ERR",
	[0x36] = "MCA_D2D_MONF_PLL_FUSA_0",
	[0x37] = "MCA_D2D_MONF_PLL_FUSA_1",
	[0x38] = "MCA_TOP0_FUSA_PLL_ERR",
	[0x39] = "MCA_BASE_FUSA_PLL_ERR",
	[0x40] = "MCA_UCIE_RESET_BUS_STOP_ON_ERROR",
	[0x41] = "MCA_RSRC_ADAPT_0_ERR",
	[0x42] = "MCA_RSC_ADAPT_1_ERR",
	[0x45] = "MCA_TOP1_FUSA_PLL_ERR",
	[0x46] = "MCA_TOP2_FUSA_PLL_ERR",
	[0x47] = "MCA_TOP3_FUSA_PLL_ERR",
	[0x48] = "UC_PATCH_LOAD_ACK_ERROR",
};

static char *cbb_punit_12[] = {
	[0x01] = "MCA_UC_IRAM_DOUBLE_ERROR",
	[0x02] = "MCA_UC_DOUBLE_EXCEPTION_ERROR",
	[0x03] = "MCA_UC_TELEM_SRAM_ERROR",
	[0x04] = "MCA_UC_DRAM_DOUBLE_ERROR",
	[0x05] = "MCA_UC_TPMI_SRAM_ERROR",
};

static char *cbb_punit_20[] = {
	[0x01] = "MCA_FIVR_HI_YELLOW",
	[0x02] = "MCA_FIVR_HI_RED",
};

static struct field cbb_punit0[] = {
	FIELD(0, cbb_punit_0),
	{}
};

static struct field cbb_punit8[] = {
	FIELD(0, cbb_punit_8),
	{}
};

static struct field cbb_punit12[] = {
	FIELD(0, cbb_punit_12),
	{}
};

static struct field cbb_punit20[] = {
	FIELD(0, cbb_punit_20),
	{}
};

static char *imh_punit_0[] = {
	[0x02 ... 0x03] = "Power Management Unit microcontroller double-bit ECC error",
	[0x08 ... 0x09] = "Power Management Unit microcontroller error",
	[0x0a] = "Power Management Unit microcontroller Patch Load error",
	[0x0b] = "Power Management Unit microcontroller POReqValid error",
	[0x0c] = "Power Management Unit microcontroller RAM address error",
	[0x0d] = "Power Management Unit microcontroller RAM access error",
	[0x10] = "If MCI_MISC.MODEL_SPECIFIC_INFORMATION is set to 1, "
		 "indicates Pcode Watchdog Timer expired. "
		 "If MCI_MISC.MODEL_SPECIFIC_INFORMATION is set to 3, "
		 "indicates Power Management Unit TeleSRAM double-bit ECC error detected.",
	[0x20] = "Power Management Agent signaled Error",
	[0x30] = "Power Management Unit Microcontroller Error",
	[0xa0] = "Power Management Unit HPMSRAM double-bit ECC error detected",
	[0xb0] = "Power Management Unit TPMISRAM double-bit ECC error detected",
};

static char *imh_punit_1[] = {
	[0x09] = "MCA_TSC_DOWNLOAD_TIMEOUT",
	[0x0a] = "MCA_INVALID_XTALFREQ_RANGE",
	[0x0b] = "MCA_GPSB_TIMEOUT",
	[0x0c] = "MCA_PMSB_TIMEOUT",
	[0x0d] = "MCA_CFG_ACK_TIMEOUT",
	[0x23] = "MCA_PCU_SVID_ERROR",
	[0x35] = "MCA_SVID_LOADLINE_INVALID",
	[0x36] = "MCA_SVID_ICCMAX_INVALID",
	[0x4a] = "MCA_FIVR_PD_HARDERR",
	[0x4c] = "MCA_HPM_DOUBLE_BIT_ERROR_DETECTED",
	[0x58] = "MCA_INVALID_MEMORY_FREQUENCY",
	[0x63] = "MCA_SVID_VCCIN_PROTOCOL_ERROR",
	[0x6a] = "MCA_SPPR_TIMEOUT",
	[0x6f] = "MCA_INVALID_SID_ERROR",
	[0x70] = "MCA_INVALID_REMOTE_LEGACY_AGENT_ERROR",
	[0x71] = "MCA_INVALID_REMOTE_LT_AGENT_ERROR",
	[0x7b] = "MCA_THERMAL_SENSOR_INVALID",
	[0x7c] = "MCA_CXL_DEVICE_NO_CREDIT",
	[0x7e] = "MCA_RECOVERABLE_DIE_THERMAL_TOO_HOT",
	[0x83] = "MCA_PKGS_RECOVERABLE_RESET_PREP_ACK_TIMEOUT",
};

static struct field imh_punit0[] = {
	FIELD(0, imh_punit_0),
	{}
};

static struct field imh_punit1[] = {
	FIELD(0, imh_punit_1),
	{}
};

static char *upi_0[] = {
	[0x00 ... 0x02] = "Internal Parity Error",
	[0x05] = "Internal Parity Error",
	[0x08] = "Internal Queue Overflow Error",
	[0x09] = "UPI link layer buffer overflow",
	[0x0a] = "Internal Queue Underflow Error",
	[0x0b] = "UPI link layer buffer underflow",
	[0x0c] = "Link layer or internal credit overflow",
	[0x11 ... 0x12] = "Internal Queue Overflow Error",
	[0x13 ... 0x14] = "Internal Queue Underflow Error",
	[0x21] = "Internal Interface Error",
	[0x23] = "Invalid Message",
	[0x24] = "UPI Protocol Error",
	[0x25] = "UPI Interleave Error",
	[0x28] = "Internal Interface Error",
	[0x29] = "Link integrity or initialization error",
	[0x2a ... 0x2d] = "Power Management Transition Error - Timeout",
	[0x30] = "Received poison when poison is disabled",
	[0x31] = "Internal Interface Error",
};

static char *upi_2[] = {
	[0x00] = "Link integrity or initialization error",
	[0x03] = "Link degraded",
	[0x04 ... 0x07] = "Power Management Transition Error - Timeout",
};

static struct field upi0[] = {
	FIELD(0, upi_0),
	{}
};

static struct field upi2[] = {
	FIELD(0, upi_2),
	{}
};

static char *mcchan_0[] = {
	[0x01] = "CMI Request Address Parity Error (APPP)",
	[0x02] = "CMI Wr data parity error",
	[0x04] = "CMI Wr BE parity error",
	[0x08] = "Transient or Correctable Error for Patrol Reads",
	[0x10] = "UnCorr Patrol Scrub Error",
	[0x20] = "Nontransient or Transient ectable Error for Spare Reads",
	[0x40] = "UnCorr Spare Error",
	[0x80] = "Transient or Correctable Error for Demand or Underfill Reads",
	[0xa0] = "Uncorrectable Error for Demand or Underfill Reads",
	[0xb0] = "Poisoned Read Data when Poison Disabled",
	[0xc0] = "Read 2LM MetaData Error for Demand, Underfill, or Patrol/Spare",
};

static char *mcchan_1[] = {
	[0x02] = "WDB Read ECC Error",
	[0x04] = "WDB BE Read Parity Error",
	[0x06] = "WDB Read Persistent ECC Error",
	[0x08] = "DDR Link Fail",
	[0x09] = "Illegal incoming opcode",
};

static char *mcchan_2[] = {
	[0x00] = "DDR CAParity or WrCRC Error",
	[0x40] = "Decoder structure error",
};

static char *mcchan_4[] = {
	[0x00] = "MC internal address parity error",
};

static char *mcchan_8[] = {
	[0x13] = "CMI Credit Oversubscription Error",
	[0x14] = "CMI Total Credit Count Error",
	[0x15] = "CMI Reserved Credit Pool Error",
	[0x32] = "MC Internal Errors",
	[0x34] = "MC Tracker RDCMP RF parity error",
	[0x36] = "MC Tracker WRCMP RF parity error",
};

static struct field mcchan0[] = {
	FIELD(0, mcchan_0),
	{}
};

static struct field mcchan1[] = {
	FIELD(0, mcchan_1),
	{}
};

static struct field mcchan2[] = {
	FIELD(0, mcchan_2),
	{}
};

static struct field mcchan4[] = {
	FIELD(0, mcchan_4),
	{}
};

static struct field mcchan8[] = {
	FIELD(0, mcchan_8),
	{}
};

static int diamond_imh(struct mce_event *e)
{
	return (e->apicid >> 2) & 1;
}

static void diamond_imc_misc(struct mce_event *e)
{
	uint64_t mscod		= EXTRACT(e->status, 16, 31);
	uint32_t column		= EXTRACT(e->misc, 9, 18) << 2;
	uint32_t row		= EXTRACT(e->misc, 19, 36);
	uint32_t bank		= EXTRACT(e->misc, 37, 38);
	uint32_t bankgroup	= EXTRACT(e->misc, 39, 41);
	uint32_t fdevice_v	= EXTRACT(e->misc, 42, 42);
	uint32_t fdevice	= EXTRACT(e->misc, 44, 48);
	uint32_t subchan	= EXTRACT(e->misc, 49, 49);
	uint32_t subrank	= EXTRACT(e->misc, 51, 54);
	uint32_t chipselect	= EXTRACT(e->misc, 55, 56);
	uint32_t eccmode	= EXTRACT(e->misc, 58, 61);
	uint32_t transient	= EXTRACT(e->misc, 63, 63);

	/* For MSCOD 0800h or above, MCi_MISC[31:9] holds proprietary error information. */
	if (mscod >= 0x800)
		return;

	mce_snprintf(e->error_msg, "bank: 0x%x bankgroup: 0x%x row: 0x%x column: 0x%x\n", bank, bankgroup, row, column);
	mce_snprintf(e->error_msg, "chipselect: 0x%x subrank: 0x%x subchan: 0x%x\n", chipselect, subrank, subchan);

	if (transient) {
		mce_snprintf(e->error_msg, "transient\n");
		return;
	}

	if (fdevice_v)
		mce_snprintf(e->error_msg, "failed device: 0x%x\n", fdevice);

	mce_snprintf(e->error_msg, "ecc mode: ");
	switch (eccmode) {
	case 1:
		mce_snprintf(e->error_msg, "SDDC 128b 1LM\n");
		break;
	case 2:
		mce_snprintf(e->error_msg, "SDDC 125b 1LM\n");
		break;
	case 3:
		mce_snprintf(e->error_msg, "SDDC 125b 2LM\n");
		break;
	case 4:
		mce_snprintf(e->error_msg, "ADDDC 80b 1LM\n");
		break;
	case 5:
		mce_snprintf(e->error_msg, "ADDDC 80b 2LM\n");
		break;
	case 9:
		mce_snprintf(e->error_msg, "5x8 128b 1LM\n");
		break;
	case 10:
		mce_snprintf(e->error_msg, "5x8 125b 1LM\n");
		break;
	case 11:
		mce_snprintf(e->error_msg, "5x8 125b 2LM\n");
		break;
	default:
		mce_snprintf(e->error_msg, "Invalid/unknown ECC mode\n");
	}
}

static void diamond_memerr_misc(struct mce_event *e, int *channel, int *dimm)
{
	/* Check this is a memory error */
	if (!test_prefix(7, e->status & 0xefff))
		return;

	if (e->bank < 19 || e->bank > 26)
		return;

	channel[0] = e->bank - 19 + 8 * diamond_imh(e);
}

void diamond_decode_model(enum cputype cputype, struct ras_events *ras, struct mce_event *e)
{
	uint64_t f, status = e->status;
	int channel = -1, dimm = -1;
	int imh = diamond_imh(e);

	switch (e->bank) {
	case 4:
		mce_snprintf(e->error_msg, "CBB Punit: ");

		if (EXTRACT(status, 0, 15) == 0x040b) {
			mce_snprintf(e->error_msg, "Scan-at-Field Error\n");
			break;
		}

		f = EXTRACT(status, 16, 23);
		switch (EXTRACT(status, 24, 31)) {
		case 0x00:
			decode_bitfield(e, f, cbb_punit0);
			break;
		case 0x08:
			decode_bitfield(e, f, cbb_punit8);
			break;
		case 0x0c:
			decode_bitfield(e, f, cbb_punit12);
			break;
		case 0x14:
			decode_bitfield(e, f, cbb_punit20);
			break;
		}
		break;
	case 11:
		mce_snprintf(e->error_msg, "IMH%d Punit: ", imh);

		f = EXTRACT(status, 24, 31);
		if (f)
			decode_bitfield(e, f, imh_punit1);
		else
			decode_bitfield(e, EXTRACT(status, 16, 23), imh_punit0);
		break;
	case 18:
		mce_snprintf(e->error_msg, "UPI: ");

		f = EXTRACT(status, 16, 23);
		switch (EXTRACT(status, 24, 31)) {
		case 0x00:
			decode_bitfield(e, f, upi0);
			break;
		case 0x02:
			decode_bitfield(e, f, upi2);
			break;
		}
		break;
	case 19 ... 26:
		mce_snprintf(e->error_msg, "IMH%d MCCHAN: ", imh);

		f = EXTRACT(status, 16, 23);
		switch (EXTRACT(status, 24, 31)) {
		case 0x00:
			decode_bitfield(e, f, mcchan0);
			break;
		case 0x01:
			decode_bitfield(e, f, mcchan1);
			break;
		case 0x02:
			decode_bitfield(e, f, mcchan2);
			break;
		case 0x04:
			decode_bitfield(e, f, mcchan4);
			break;
		case 0x08:
			decode_bitfield(e, f, mcchan8);
			break;
		}

		/* Decode MISC register if MISCV and OTHER_INFO[1] are both set. */
		if (EXTRACT(status, 59, 59) && EXTRACT(status, 33, 33))
			diamond_imc_misc(e);
		break;
	}

	/* Parse the reported channel and DIMM. */
	diamond_memerr_misc(e, &channel, &dimm);
	if (channel != -1)
		mce_snprintf(e->error_msg, "channel: %d", channel);
	if (dimm != -1)
		mce_snprintf(e->error_msg, "dimm: %d", dimm);
}
