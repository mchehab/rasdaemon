// SPDX-License-Identifier: GPL-2.0

/*
 * The code below came from Tony Luck's mcelog code,
 * released under GNU Public General License, v.2
 *
 * Copyright (C) 2022 Intel Corporation
 * Decode Intel graniterapids, {sierra,clearwater}forest
 * specific machine check errors.
 */

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "bitfield.h"
#include "ras-mce-handler.h"

static char *upi_2[] = {
	[0x00] = "UC Phy Initialization Failure (NumInit)",
	[0x01] = "UC Phy Detected Drift Buffer Alarm",
	[0x02] = "UC Phy Detected Latency Buffer Rollover",
	[0x10] = "UC LL Rx detected CRC error: unsuccessful LLR (entered Abort state)",
	[0x11] = "UC LL Rx Unsupported/Undefined packet",
	[0x12] = "UC LL or Phy Control Error",
	[0x13] = "UC LL Rx Parameter Exception",
	[0x14] = "UC LL TDX Failure",
	[0x15] = "UC LL SGX Failure",
	[0x16] = "UC LL Tx SDC Parity Error",
	[0x17] = "UC LL Rx SDC Parity Error",
	[0x18] = "UC LL FLE Failure",
	[0x1F] = "UC LL Detected Control Error",
	[0x20] = "COR Phy Initialization Abort",
	[0x21] = "COR Phy Inband Reset",
	[0x22] = "COR Phy Lane failure, recovery in x8 width",
	[0x23] = "COR Phy L0c error corrected without Phy reset",
	[0x24] = "COR Phy L0c error triggering Phy reset",
	[0x25] = "COR Phy L0p exit error corrected with reset",
	[0x30] = "COR LL Rx detected CRC error: successful LLR without Phy Reinit",
	[0x31] = "COR LL Rx detected CRC error: successful LLR with Phy Reinit",
};

static struct field upi2[] = {
	FIELD(0, upi_2),
	{}
};

static char *punit_errs_1[] = {
	[0x02 ... 0x3] = "Power Management Unit microcontroller double-bit ECC error",
	[0x08 ... 0x9] = "Power Management Unit microcontroller error",
	[0x0a] = "Power Management Unit microcontroller patch load error",
	[0x0b] = "Power Management Unit microcontroller POReqValid error",
	[0x10] = "Power Management Unit microcontroller TeleSRAM double-bit ECC error",
	[0x20] = "Power Management Agent signaled error",
	[0x80] = "S3M signaled error",
	[0xa0] = "Power Management Unit HPMSRAM double-bit ECC error detected",
	[0xb0] = "Power Management Unit TPMISRAM double-bit ECC error detected",
};

static struct field punit1[] = {
	FIELD(0, punit_errs_1),
	{}
};

static char *punit_errs_2[] = {
	[0x09] = "MCA_TSC_DOWNLOAD_TIMEOUT: TSC download timed out",
	[0x0b] = "MCA_GPSB_TIMEOUT: GPSB does not respond within timeout value",
	[0x0c] = "MCA_PMSB_TIMEOUT: PMSB does not respond within timeout value",
	[0x10] = "MCA_PMAX_CALIB_ERROR: PMAX calibration error",
	[0x1a] = "MCA_DISP_RUN_BUSY_TIMEOUT: Dispatcher busy beyond timeout",
	[0x1d] = "MCA_MORE_THAN_ONE_LT_AGENT: During Boot Mode Processing, >1 LT Agent detected",
	[0x23] = "MCA_PCU_SVID_ERROR: SVID error",
	[0x35] = "MCA_SVID_LOADLINE_INVALID: Invalid SVID VCCIN Loadline resistance value",
	[0x36] = "MCA_SVID_ICCMAX_INVALID: Invalid SVID ICCMAX value",
	[0x40] = "MCA_SVID_VIDMAX_INVALID: Invalid SVID VID_MAX value",
	[0x41] = "MCA_SVID_VDDRAMP_INVALID: Invalid ramp voltage for VDD_RAMP",
	[0x48] = "MCA_ITD_FUSE_INVALID: ITD fuse settings are not valid",
	[0x49] = "MCA_SVID_DC_LL_INVALID: ITD fuse settings are not valid",
	[0x4a] = "MCA_FIVR_PD_HARDERR: PM event has issued some FIVR Fault",
	[0x4c] = "MCA_HPM_DOUBLE_BIT_ERROR_DETECTED: Read from HPM SRAM resulted in Double bit error",
	[0x56] = "SVID_ACTIVE_VID_FUSE_ERROR: Invalid fuse values programmed for SVID Active vid",
};

static struct field punit2[] = {
	FIELD(0, punit_errs_2),
	{}
};

static char *b2cmi_1[] = {
	[0x01] = "Read ECC error",
	[0x02] = "Bucket1 error",
	[0x03] = "Tracker Parity error",
	[0x04] = "Security mismatch",
	[0x07] = "Read completion parity error",
	[0x08] = "Response parity error",
	[0x09] = "Timeout error",
	[0x0a] = "Address parity error",
	[0x0c] = "CMI credit over subscription error",
	[0x0d] = "SAI mismatch error",
};

static struct field b2cmi1[] = {
	FIELD(0, b2cmi_1),
	{}
};

static char *mcchan_0[] = {
	[0x01] = "Address Parity Error (APPP)",
	[0x02] = "CMI Wr data parity Error on sCH0",
	[0x03] = "CMI Uncorr/Corr ECC error on sCH0",
	[0x04] = "CMI Wr BE parity Error on sCH0",
	[0x05] = "CMI Wr MAC parity Error on sCH0",
	[0x08] = "Corr Patrol Scrub Error",
	[0x10] = "UnCorr Patrol Scrub Error",
	[0x20] = "Corr Spare Error",
	[0x40] = "UnCorr Spare Error",
	[0x80] = "Transient or Correctable Error for Demand or Underfill Reads",
	[0xa0] = "Uncorrectable Error for Demand or Underfill Reads",
	[0xb0] = "Poison was read from memory when poison was disabled in memory controller",
	[0xc0] = "Read 2LM MetaData Error",
};

static char *mcchan_1[] = {
	[0x00] = "WDB Read Parity Error on sCH0",
	[0x02] = "WDB Read Uncorr/Corr ECC Error on sCH0",
	[0x04] = "WDB BE Read Parity Error on sCH0",
	[0x06] = "WDB Read Persistent Corr ECC Error on sCH0",
	[0x08] = "DDR Link Fail",
	[0x09] = "Illegal incoming opcode",
};

static char *mcchan_2[] = {
	[0x00] = "DDR CAParity or WrCRC Error",
};

static char *mcchan_4[] = {
	[0x00] = "Scheduler address parity error",
};

static char *mcchan_8[] = {
	[0x32] = "MC Internal Errors",
	[0x33] = "MCTracker Address RF parity error",
};

static char *mcchan_32[] = {
	[0x02] = "CMI Wr data parity Error on sCH1",
	[0x03] = "CMI Uncorr/Corr ECC error on sCH1",
	[0x04] = "CMI Wr BE parity Error on sCH1",
	[0x05] = "CMI Wr MAC parity Error on sCH1",
};

static char *mcchan_33[] = {
	[0x00] = "WDB Read Parity Error on sCH1",
	[0x02] = "WDB Read Uncorr/Corr ECC Error on sCH1",
	[0x04] = "WDB BE Read Parity Error on sCH1",
	[0x06] = "WDB Read Persistent Corr ECC Error on sCH1",
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

static struct field mcchan32[] = {
	FIELD(0, mcchan_32),
	{}
};

static struct field mcchan33[] = {
	FIELD(0, mcchan_33),
	{}
};

static void granite_imc_misc(struct mce_event *e)
{
	uint64_t mscod = EXTRACT(e->status, 16, 31);
	uint32_t column = EXTRACT(e->misc, 9, 18) << 2;
	uint32_t row = EXTRACT(e->misc, 19, 36);
	uint32_t bank = EXTRACT(e->misc, 37, 38);
	uint32_t bankgroup = EXTRACT(e->misc, 39, 41);
	uint32_t fdevice = EXTRACT(e->misc, 43, 48);
	uint32_t subrank = EXTRACT(e->misc, 51, 54);
	uint32_t chipselect = EXTRACT(e->misc, 55, 57);
	uint32_t eccmode = EXTRACT(e->misc, 58, 61);
	uint32_t transient = EXTRACT(e->misc, 63, 63);

	if (mscod >= 0x800 && mscod <= 0x82f)
		return;

	mce_snprintf(e->error_msg, "bank: 0x%x bankgroup: 0x%x row: 0x%x column: 0x%x\n", bank, bankgroup, row, column);
	if (!transient)
		mce_snprintf(e->error_msg, "failed device: 0x%x\n", fdevice);
	mce_snprintf(e->error_msg, "chipselect: 0x%x subrank: 0x%x\n", chipselect, subrank);
	mce_snprintf(e->error_msg, "ecc mode: ");
	switch (eccmode) {
	case 1:
		mce_snprintf(e->error_msg, "SDDC 128b 1LM\n");
		break;
	case 2:
		mce_snprintf(e->error_msg, "SDDC 125b 1LM\n");
		break;
	case 3:
		mce_snprintf(e->error_msg, "SDDC 96b 1LM\n");
		break;
	case 4:
		mce_snprintf(e->error_msg, "SDDC 96b 2LM\n");
		break;
	case 5:
		mce_snprintf(e->error_msg, "ADDDC 80b 1LM\n");
		break;
	case 6:
		mce_snprintf(e->error_msg, "ADDDC 80b 2LM\n");
		break;
	case 7:
		mce_snprintf(e->error_msg, "ADDDC 64b 1LM\n");
		break;
	case 8:
		mce_snprintf(e->error_msg, "9x4 61b 1LM\n");
		break;
	case 9:
		mce_snprintf(e->error_msg, "9x4 32b 1LM\n");
		break;
	case 10:
		mce_snprintf(e->error_msg, "9x4 32b 2LM\n");
		break;
	default:
		mce_snprintf(e->error_msg, "Invalid/unknown ECC mode\n");
		break;
	}
	if (transient)
		mce_snprintf(e->error_msg, "transient\n");
}

static void granite_memerr_misc(struct mce_event *e, int *channel);

void granite_decode_model(enum cputype cputype, struct ras_events *ras, struct mce_event *e)
{
	uint64_t f;
	int channel = -1;

	switch (e->bank) {
	case 5: /* UPI */
		mce_snprintf(e->error_msg, "UPI: ");
		f = EXTRACT(e->status, 16, 31);
		decode_bitfield(e, f, upi2);
		break;

	case 6: /* Punit */
		mce_snprintf(e->error_msg, "Punit: ");
		f = EXTRACT(e->status, 16, 23);
		decode_bitfield(e, f, punit1);
		f = EXTRACT(e->status, 24, 31);
		decode_bitfield(e, f, punit2);
		break;

	case 12: /* B2CMI */
		mce_snprintf(e->error_msg, "B2CMI: ");
		f = EXTRACT(e->status, 16, 31);
		decode_bitfield(e, f, b2cmi1);
		break;

	case 13 ... 24: /* MCCHAN */
		mce_snprintf(e->error_msg, "MCCHAN: ");
		f = EXTRACT(e->status, 16, 23);
		switch (EXTRACT(e->status, 24, 31)) {
		case 0:
			decode_bitfield(e, f, mcchan0);
			break;
		case 1:
			decode_bitfield(e, f, mcchan1);
			break;
		case 2:
			decode_bitfield(e, f, mcchan2);
			break;
		case 4:
			decode_bitfield(e, f, mcchan4);
			break;
		case 8:
			decode_bitfield(e, f, mcchan8);
			break;
		case 32:
			decode_bitfield(e, f, mcchan32);
			break;
		case 33:
			decode_bitfield(e, f, mcchan33);
			break;
		}

		/* Decode MISC register if MISCV and OTHER_INFO[1] are both set */
		if (EXTRACT(e->status, 59, 59) && EXTRACT(e->status, 33, 33))
			granite_imc_misc(e);
		break;
	}

	/*
	 * Parse the reported channel
	 */
	granite_memerr_misc(e, &channel);
	if (channel == -1)
		return;
}

static void granite_memerr_misc(struct mce_event *e, int *channel)
{
	uint64_t status = e->status;
	unsigned int chan;

	/* Check this is a memory error */
	if (!test_prefix(7, status & 0xefff))
		return;

	chan = EXTRACT(status, 0, 3);
	if (chan == 0xf)
		return;

	if (e->bank < 13 || e->bank > 24)
		return;

	channel[0] = e->bank - 13;
}
