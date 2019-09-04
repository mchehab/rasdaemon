/*
 * The code below came from Tony Luck mcelog code,
 * released under GNU Public General License, v.2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
*/

#include <string.h>
#include <stdio.h>

#include "ras-mce-handler.h"
#include "bitfield.h"


/* See IA32 SDM Vol3B Table 16-20 */

static char *pcu_1[] = {
	[0x00] = "No Error",
	[0x09] = "MC_MESSAGE_CHANNEL_TIMEOUT",
	[0x0D] = "MC_IMC_FORCE_SR_S3_TIMEOUT",
	[0x0E] = "MC_CPD_UNCPD_SD_TIMEOUT",
	[0x13] = "MC_DMI_TRAINING_TIMEOUT",
	[0x15] = "MC_DMI_CPU_RESET_ACK_TIMEOUT",
	[0x1E] = "MC_VR_ICC_MAX_LT_FUSED_ICC_MAX",
	[0x25] = "MC_SVID_COMMAN_TIMEOUT",
	[0x29] = "MC_VR_VOUT_MAC_LT_FUSED_SVID",
	[0x2B] = "MC_PKGC_WATCHDOG_HANG_CBZ_DOWN",
	[0x2C] = "MC_PKGC_WATCHDOG_HANG_CBZ_UP",
	[0x39] = "MC_PKGC_WATCHDOG_HANG_C3_UP_SF",
	[0x44] = "MC_CRITICAL_VR_FAILED",
	[0x45] = "MC_ICC_MAX_NOTSUPPORTED",
	[0x46] = "MC_VID_RAMP_DOWN_FAILED",
	[0x47] = "MC_EXCL_MODE_NO_PMREQ_CMP",
	[0x48] = "MC_SVID_READ_REG_ICC_MAX_FAILED",
	[0x49] = "MC_SVID_WRITE_REG_VOUT_MAX_FAILED",
	[0x4B] = "MC_BOOT_VID_TIMEOUT_DRAM_0",
	[0x4C] = "MC_BOOT_VID_TIMEOUT_DRAM_1",
	[0x4D] = "MC_BOOT_VID_TIMEOUT_DRAM_2",
	[0x4E] = "MC_BOOT_VID_TIMEOUT_DRAM_3",
	[0x4F] = "MC_SVID_COMMAND_ERROR",
	[0x52] = "MC_FIVR_CATAS_OVERVOL_FAULT",
	[0x53] = "MC_FIVR_CATAS_OVERCUR_FAULT",
	[0x57] = "MC_SVID_PKGC_REQUEST_FAILED",
	[0x58] = "MC_SVID_IMON_REQUEST_FAILED",
	[0x59] = "MC_SVID_ALERT_REQUEST_FAILED",
	[0x60] = "MC_INVALID_PKGS_REQ_PCH",
	[0x61] = "MC_INVALID_PKGS_REQ_QPI",
	[0x62] = "MC_INVALID_PKGS_RSP_QPI",
	[0x63] = "MC_INVALID_PKGS_RSP_PCH",
	[0x64] = "MC_INVALID_PKG_STATE_CONFIG",
	[0x67] = "MC_HA_IMC_RW_BLOCK_ACK_TIMEOUT",
	[0x68] = "MC_IMC_RW_SMBUS_TIMEOUT",
	[0x69] = "MC_HA_FAILSTS_CHANGE_DETECTED",
	[0x6A] = "MC_MSGCH_PMREQ_CMP_TIMEOUT",
	[0x70] = "MC_WATCHDOG_TIMEOUT_PKGC_SLAVE",
	[0x71] = "MC_WATCHDOG_TIMEOUT_PKGC_MASTER",
	[0x72] = "MC_WATCHDOG_TIMEOUT_PKGS_MASTER",
	[0x7C] = "MC_BIOS_RST_CPL_INVALID_SEQ",
	[0x7D] = "MC_MORE_THAN_ONE_TXT_AGENT",
	[0x81] = "MC_RECOVERABLE_DIE_THERMAL_TOO_HOT"
};

static struct field pcu_mc4[] = {
	FIELD(24, pcu_1),
	{}
};

/* See IA32 SDM Vol3B Table 16-21 */

static char *qpi[] = {
	[0x02] = "Intel QPI physical layer detected drift buffer alarm",
	[0x03] = "Intel QPI physical layer detected latency buffer rollover",
	[0x10] = "Intel QPI link layer detected control error from R3QPI",
	[0x11] = "Rx entered LLR abort state on CRC error",
	[0x12] = "Unsupported or undefined packet",
	[0x13] = "Intel QPI link layer control error",
	[0x15] = "RBT used un-initialized value",
	[0x20] = "Intel QPI physical layer detected a QPI in-band reset but aborted initialization",
	[0x21] = "Link failover data self healing",
	[0x22] = "Phy detected in-band reset (no width change)",
	[0x23] = "Link failover clock failover",
	[0x30] = "Rx detected CRC error - successful LLR after Phy re-init",
	[0x31] = "Rx detected CRC error - successful LLR wihout Phy re-init",
};

static struct field qpi_mc[] = {
	FIELD(16, qpi),
	{}
};

/* See IA32 SDM Vol3B Table 16-22 */

static struct field memctrl_mc9[] = {
	SBITFIELD(16, "DDR3 address parity error"),
	SBITFIELD(17, "Uncorrected HA write data error"),
	SBITFIELD(18, "Uncorrected HA data byte enable error"),
	SBITFIELD(19, "Corrected patrol scrub error"),
	SBITFIELD(20, "Uncorrected patrol scrub error"),
	SBITFIELD(21, "Corrected spare error"),
	SBITFIELD(22, "Uncorrected spare error"),
	SBITFIELD(23, "Corrected memory read error"),
	SBITFIELD(24, "iMC write data buffer parity error"),
	SBITFIELD(25, "DDR4 command address parity error"),
	{}
};

void hsw_decode_model(struct ras_events *ras, struct mce_event *e)
{
	uint64_t status = e->status;
	uint32_t mca = status & 0xffff;
	unsigned rank0 = -1, rank1 = -1, chan;

	switch (e->bank) {
	case 4:
		switch (EXTRACT(status, 0, 15) & ~(1ull << 12)) {
		case 0x402: case 0x403:
			mce_snprintf(e->mcastatus_msg, "PCU Internal Errors");
			break;
		case 0x406:
			mce_snprintf(e->mcastatus_msg, "Intel TXT Errors");
			break;
		case 0x407:
			mce_snprintf(e->mcastatus_msg, "Other UBOX Internal Errors");
			break;
		}
		if (EXTRACT(status, 16, 17) && !EXTRACT(status, 18, 19))
			mce_snprintf(e->error_msg, "PCU Internal error");
		decode_bitfield(e, status, pcu_mc4);
		break;
	case 5:
	case 20:
	case 21:
		decode_bitfield(e, status, qpi_mc);
		break;
	case 9: case 10: case 11: case 12:
	case 13: case 14: case 15: case 16:
		decode_bitfield(e, status, memctrl_mc9);
		break;
	}

	/*
	 * Memory error specific code. Returns if the error is not a MC one
	 */

	/* Check if the error is at the memory controller */
	if ((mca >> 7) != 1)
		return;

	/* Ignore unless this is an corrected extended error from an iMC bank */
	if (e->bank < 9 || e->bank > 16 || (status & MCI_STATUS_UC) ||
		!test_prefix(7, status & 0xefff))
		return;

	/*
	 * Parse the reported channel and ranks
	 */

	chan = EXTRACT(status, 0, 3);
	if (chan == 0xf)
		return;

	mce_snprintf(e->mc_location, "memory_channel=%d", chan);

	if (EXTRACT(e->misc, 62, 62)) {
		rank0 = EXTRACT(e->misc, 46, 50);
		if (EXTRACT(e->misc, 63, 63))
			rank1 = EXTRACT(e->misc, 51, 55);
	}

	/*
	 * FIXME: The conversion from rank to dimm requires to parse the
	 * DMI tables and call failrank2dimm().
	 */
	if (rank0 != -1 && rank1 != -1)
		mce_snprintf(e->mc_location, "ranks=%d and %d",
				     rank0, rank1);
	else if (rank0 != -1)
		mce_snprintf(e->mc_location, "rank=%d", rank0);
}

