/*
 * The code below came from Tony Luck's mcelog code,
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

/* See IA32 SDM Vol3B Table 16-27 */

static char *pcu_1[] = {
	[0x00] = "No Error",
	[0x0d] = "MCA_DMI_TRAINING_TIMEOUT",
	[0x0f] = "MCA_DMI_CPU_RESET_ACK_TIMEOUT",
	[0x10] = "MCA_MORE_THAN_ONE_LT_AGENT",
	[0x1e] = "MCA_BIOS_RST_CPL_INVALID_SEQ",
	[0x1f] = "MCA_BIOS_INVALID_PKG_STATE_CONFIG",
	[0x25] = "MCA_MESSAGE_CHANNEL_TIMEOUT",
	[0x27] = "MCA_MSGCH_PMREQ_CMP_TIMEOUT",
	[0x30] = "MCA_PKGC_DIRECT_WAKE_RING_TIMEOUT",
	[0x31] = "MCA_PKGC_INVALID_RSP_PCH",
	[0x33] = "MCA_PKGC_WATCHDOG_HANG_CBZ_DOWN",
	[0x34] = "MCA_PKGC_WATCHDOG_HANG_CBZ_UP",
	[0x38] = "MCA_PKGC_WATCHDOG_HANG_C3_UP_SF",
	[0x40] = "MCA_SVID_VCCIN_VR_ICC_MAX_FAILURE",
	[0x41] = "MCA_SVID_COMMAND_TIMEOUT",
	[0x42] = "MCA_SVID_VCCIN_VR_VOUT_FAILURE",
	[0x43] = "MCA_SVID_CPU_VR_CAPABILITY_ERROR",
	[0x44] = "MCA_SVID_CRITICAL_VR_FAILED",
	[0x45] = "MCA_SVID_SA_ITD_ERROR",
	[0x46] = "MCA_SVID_READ_REG_FAILED",
	[0x47] = "MCA_SVID_WRITE_REG_FAILED",
	[0x48] = "MCA_SVID_PKGC_INIT_FAILED",
	[0x49] = "MCA_SVID_PKGC_CONFIG_FAILED",
	[0x4a] = "MCA_SVID_PKGC_REQUEST_FAILED",
	[0x4b] = "MCA_SVID_IMON_REQUEST_FAILED",
	[0x4c] = "MCA_SVID_ALERT_REQUEST_FAILED",
	[0x4d] = "MCA_SVID_MCP_VR_ABSENT_OR_RAMP_ERROR",
	[0x4e] = "MCA_SVID_UNEXPECTED_MCP_VR_DETECTED",
	[0x51] = "MCA_FIVR_CATAS_OVERVOL_FAULT",
	[0x52] = "MCA_FIVR_CATAS_OVERCUR_FAULT",
	[0x58] = "MCA_WATCHDOG_TIMEOUT_PKGC_SLAVE",
	[0x59] = "MCA_WATCHDOG_TIMEOUT_PKGC_MASTER",
	[0x5a] = "MCA_WATCHDOG_TIMEOUT_PKGS_MASTER",
	[0x61] = "MCA_PKGS_CPD_UNCPD_TIMEOUT",
	[0x63] = "MCA_PKGS_INVALID_REQ_PCH",
	[0x64] = "MCA_PKGS_INVALID_REQ_INTERNAL",
	[0x65] = "MCA_PKGS_INVALID_RSP_INTERNAL",
	[0x6b] = "MCA_PKGS_SMBUS_VPP_PAUSE_TIMEOUT",
	[0x81] = "MCA_RECOVERABLE_DIE_THERMAL_TOO_HOT",
};

static struct field pcu_mc4[] = {
	FIELD(24, pcu_1),
	{}
};

/* See IA32 SDM Vol3B Table 16-28 */

static char *upi[] = {
	[0x00] = "UC Phy Initialization Failure",
	[0x01] = "UC Phy detected drift buffer alarm",
	[0x02] = "UC Phy detected latency buffer rollover",
	[0x10] = "UC LL Rx detected CRC error: unsuccessful LLR: entered abort state",
	[0x11] = "UC LL Rx unsupported or undefined packet",
	[0x12] = "UC LL or Phy control error",
	[0x13] = "UC LL Rx parameter exchange exception",
	[0x1F] = "UC LL detected control error from the link-mesh interface",
	[0x20] = "COR Phy initialization abort",
	[0x21] = "COR Phy reset",
	[0x22] = "COR Phy lane failure, recovery in x8 width",
	[0x23] = "COR Phy L0c error corrected without Phy reset",
	[0x24] = "COR Phy L0c error triggering Phy Reset",
	[0x25] = "COR Phy L0p exit error corrected with Phy reset",
	[0x30] = "COR LL Rx detected CRC error - successful LLR without Phy Reinit",
	[0x31] = "COR LL Rx detected CRC error - successful LLR with Phy Reinit",
};

static struct field upi_mc[] = {
	FIELD(16, upi),
	{}
};

/* These apply to MSCOD 0x12 "UC LL or Phy control error" */
static struct field upi_0x12[] = {
	SBITFIELD(22, "Phy Control Error"),
	SBITFIELD(23, "Unexpected Retry.Ack flit"),
	SBITFIELD(24, "Unexpected Retry.Req flit"),
	SBITFIELD(25, "RF parity error"),
	SBITFIELD(26, "Routeback Table error"),
	SBITFIELD(27, "unexpected Tx Protocol flit (EOP, Header or Data)"),
	SBITFIELD(28, "Rx Header-or-Credit BGF credit overflow/underflow"),
	SBITFIELD(29, "Link Layer Reset still in progress when Phy enters L0"),
	SBITFIELD(30, "Link Layer reset initiated while protocol traffic not idle"),
	SBITFIELD(31, "Link Layer Tx Parity Error"),
	{}
};

/* See IA32 SDM Vol3B Table 16-29 */

static struct field mc_bits[] = {
	SBITFIELD(16, "Address parity error"),
	SBITFIELD(17, "HA write data parity error"),
	SBITFIELD(18, "HA write byte enable parity error"),
	SBITFIELD(19, "Corrected patrol scrub error"),
	SBITFIELD(20, "Uncorrected patrol scrub error"),
	SBITFIELD(21, "Corrected spare error"),
	SBITFIELD(22, "Uncorrected spare error"),
	SBITFIELD(23, "Any HA read error"),
	SBITFIELD(24, "WDB read parity error"),
	SBITFIELD(25, "DDR4 command address parity error"),
	SBITFIELD(26, "Uncorrected address parity error"),
	{}
};

static char *mc_0x8xx[] = {
	[0x0] = "Unrecognized request type",
	[0x1] = "Read response to an invalid scoreboard entry",
	[0x2] = "Unexpected read response",
	[0x3] = "DDR4 completion to an invalid scoreboard entry",
	[0x4] = "Completion to an invalid scoreboard entry",
	[0x5] = "Completion FIFO overflow",
	[0x6] = "Correctable parity error",
	[0x7] = "Uncorrectable error",
	[0x8] = "Interrupt received while outstanding interrupt was not ACKed",
	[0x9] = "ERID FIFO overflow",
	[0xa] = "Error on Write credits",
	[0xb] = "Error on Read credits",
	[0xc] = "Scheduler error",
	[0xd] = "Error event",
};

static struct field memctrl_mc13[] = {
	FIELD(16, mc_0x8xx),
	{}
};

/* See IA32 SDM Vol3B Table 16-30 */

static struct field m2m[] = {
	SBITFIELD(16, "MscodDataRdErr"),
	SBITFIELD(17, "Reserved"),
	SBITFIELD(18, "MscodPtlWrErr"),
	SBITFIELD(19, "MscodFullWrErr"),
	SBITFIELD(20, "MscodBgfErr"),
	SBITFIELD(21, "MscodTimeout"),
	SBITFIELD(22, "MscodParErr"),
	SBITFIELD(23, "MscodBucket1Err"),
	{}
};

void skylake_s_decode_model(struct ras_events *ras, struct mce_event *e)
{
	uint64_t status = e->status;
	uint32_t mca = status & 0xffff;
	unsigned rank0 = -1, rank1 = -1, chan;

	switch (e->bank) {
	case 4:
		switch (EXTRACT(status, 0, 15) & ~(1ull << 12)) {
		case 0x402: case 0x403:
			mce_snprintf(e->mcastatus_msg, "Internal errors ");
			break;
		case 0x406:
			mce_snprintf(e->mcastatus_msg, "Intel TXT errors ");
			break;
		case 0x407:
			mce_snprintf(e->mcastatus_msg, "Other UBOX Internal errors ");
			break;
		}
		if (EXTRACT(status, 16, 19))
			mce_snprintf(e->mcastatus_msg, "PCU internal error ");
		decode_bitfield(e, status, pcu_mc4);
		break;
	case 5:
	case 12:
	case 19:
		mce_snprintf(e->mcastatus_msg, "UPI: ");
		decode_bitfield(e, status, upi_mc);
		if (EXTRACT(status, 16, 21) == 0x12)
			decode_bitfield(e, status, upi_0x12);
		break;
	case 7: case 8:
		mce_snprintf(e->mcastatus_msg, "M2M: ");
		decode_bitfield(e, status, m2m);
		break;
	case 13: case 14: case 15:
	case 16: case 17: case 18:
		mce_snprintf(e->mcastatus_msg, "MemCtrl: ");
		if (EXTRACT(status, 27, 27))
			decode_bitfield(e, status, memctrl_mc13);
		else
			decode_bitfield(e, status, mc_bits);
		break;
	}

	/*
	 * Memory error specific code. Returns if the error is not a MC one
	 */

	/* Check if the error is at the memory controller */
	if ((mca >> 7) != 1)
		return;

	/* Ignore unless this is an corrected extended error from an iMC bank */
	if (e->bank < 13 || e->bank > 18 || (status & MCI_STATUS_UC) ||
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
