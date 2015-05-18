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

/* See IA32 SDM Vol3B Table 16-17 */

static char *pcu_1[] = {
	[0] = "No error",
	[1] = "Non_IMem_Sel",
	[2] = "I_Parity_Error",
	[3] = "Bad_OpCode",
	[4] = "I_Stack_Underflow",
	[5] = "I_Stack_Overflow",
	[6] = "D_Stack_Underflow",
	[7] = "D_Stack_Overflow",
	[8] = "Non-DMem_Sel",
	[9] = "D_Parity_Error"
};

static char *pcu_2[] = {
	[0x00] = "No Error",
	[0x0D] = "MC_IMC_FORCE_SR_S3_TIMEOUT",
	[0x0E] = "MC_MC_CPD_UNCPD_ST_TIMEOUT",
	[0x0F] = "MC_PKGS_SAFE_WP_TIMEOUT",
	[0x43] = "MC_PECI_MAILBOX_QUIESCE_TIMEOUT",
	[0x44] = "MC_CRITICAL_VR_FAILED",
	[0x45] = "MC_ICC_MAX-NOTSUPPORTED",
	[0x5C] = "MC_MORE_THAN_ONE_LT_AGENT",
	[0x60] = "MC_INVALID_PKGS_REQ_PCH",
	[0x61] = "MC_INVALID_PKGS_REQ_QPI",
	[0x62] = "MC_INVALID_PKGS_RES_QPI",
	[0x63] = "MC_INVALID_PKGC_RES_PCH",
	[0x64] = "MC_INVALID_PKG_STATE_CONFIG",
	[0x70] = "MC_WATCHDG_TIMEOUT_PKGC_SLAVE",
	[0x71] = "MC_WATCHDG_TIMEOUT_PKGC_MASTER",
	[0x72] = "MC_WATCHDG_TIMEOUT_PKGS_MASTER",
	[0x7A] = "MC_HA_FAILSTS_CHANGE_DETECTED",
	[0x7B] = "MC_PCIE_R2PCIE-RW_BLOCK_ACK_TIMEOUT",
	[0x81] = "MC_RECOVERABLE_DIE_THERMAL_TOO_HOT",
};

static struct field pcu_mc4[] = {
	FIELD(16, pcu_1),
	FIELD(24, pcu_2),
	{}
};

/* See IA32 SDM Vol3B Table 16-18 */

static char *memctrl_1[] = {
	[0x001] = "Address parity error",
	[0x002] = "HA Wrt buffer Data parity error",
	[0x004] = "HA Wrt byte enable parity error",
	[0x008] = "Corrected patrol scrub error",
	[0x010] = "Uncorrected patrol scrub error",
	[0x020] = "Corrected spare error",
	[0x040] = "Uncorrected spare error",
	[0x080] = "Corrected memory read error",
	[0x100] = "iMC, WDB, parity errors",
};

static struct field memctrl_mc9[] = {
	FIELD(16, memctrl_1),
	{}
};

void ivb_decode_model(struct ras_events *ras, struct mce_event *e)
{
	struct mce_priv *mce = ras->mce_priv;
	uint64_t status = e->status;
	uint32_t mca    = status & 0xffff;
	unsigned	rank0 = -1, rank1 = -1, chan;

	switch (e->bank) {
	case 4:
//		Wprintf("PCU: ");
		decode_bitfield(e, e->status, pcu_mc4);
//		Wprintf("\n");
		break;
	case 5:
		if (mce->cputype == CPU_IVY_BRIDGE_EPEX) {
			/* MCACOD already decoded */
			mce_snprintf(e->bank_name, "QPI");
		}
		break;
	case 9: case 10: case 11: case 12:
	case 13: case 14: case 15: case 16:
//		Wprintf("MemCtrl: ");
		decode_bitfield(e, e->status, memctrl_mc9);
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

	if (EXTRACT(e->misc, 62, 62))
		rank0 = EXTRACT(e->misc, 46, 50);

	if (EXTRACT(e->misc, 63, 63))
		rank1 = EXTRACT(e->misc, 51, 55);

	/*
	 * FIXME: The conversion from rank to dimm requires to parse the
	 * DMI tables and call failrank2dimm().
	 */
	if (rank0 >= 0 && rank1 >= 0)
		mce_snprintf(e->mc_location, "ranks=%d and %d",
				     rank0, rank1);
	else if (rank0 >= 0)
		mce_snprintf(e->mc_location, "rank=%d", rank0);
	else
		mce_snprintf(e->mc_location, "rank=%d", rank1);
}

/*
 * Ivy Bridge EP and EX processors (family 6, model 62) support additional
 * logging for corrected errors in the integrated memory controller (IMC)
 * banks. The mode is off by default, but can be enabled by setting the
 * "MemError Log Enable" * bit in MSR_ERROR_CONTROL (MSR 0x17f).
 * The SDM leaves it as an exercise for the reader to convert the
 * faling rank to a DIMM slot.
 */
#if 0
static int failrank2dimm(unsigned failrank, int socket, int channel)
{
	switch (failrank) {
	case 0: case 1: case 2: case 3:
		return 0;
	case 4: case 5:
		return 1;
	case 6: case 7:
		if (get_memdimm(socket, channel, 2, 0))
			return 2;
		else
			return 1;
	}
	return -1;
}
#endif

