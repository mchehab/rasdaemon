/*
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

static struct field memctrl_mc7[] = {
	SBITFIELD(16, "CA Parity error"),
	SBITFIELD(17, "Internal Parity error except WDB"),
	SBITFIELD(18, "Internal Parity error from WDB"),
	SBITFIELD(19, "Correctable Patrol Scrub"),
	SBITFIELD(20, "Uncorrectable Patrol Scrub"),
	SBITFIELD(21, "Spare Correctable Error"),
	SBITFIELD(22, "Spare UC Error"),
	SBITFIELD(23, "CORR Chip fail even MC only, 4 bit burst error EDC only"),
	{}
};

void knl_decode_model(struct ras_events *ras, struct mce_event *e)
{
	uint64_t status = e->status;
	uint32_t mca = status & 0xffff;
	unsigned rank0 = -1, rank1 = -1, chan = 0;

	switch (e->bank) {
	case 5:
		switch (EXTRACT(status, 0, 15)) {
		case 0x402:
			mce_snprintf(e->mcastatus_msg, "PCU Internal Errors");
			break;
		case 0x403:
			mce_snprintf(e->mcastatus_msg, "VCU Internal Errors");
			break;
		case 0x407:
			mce_snprintf(e->mcastatus_msg,
				     "Other UBOX Internal Errors");
			break;
		}
		break;
	case 7:
	case 8:
	case 9:
	case 10:
	case 11:
	case 12:
	case 13:
	case 14:
	case 15:
	case 16:
		if ((EXTRACT(status, 0, 15)) == 0x5) {
			mce_snprintf(e->mcastatus_msg, "Internal Parity error");
		} else {
			chan = (EXTRACT(status, 0, 3)) + 3 * (e->bank == 15);
			switch (EXTRACT(status, 4, 7)) {
			case 0x0:
				mce_snprintf(e->mcastatus_msg,
					     "Undefined request on channel %d",
					     chan);
				break;
			case 0x1:
				mce_snprintf(e->mcastatus_msg,
					     "Read on channel %d", chan);
				break;
			case 0x2:
				mce_snprintf(e->mcastatus_msg,
					     "Write on channel %d", chan);
				break;
			case 0x3:
				mce_snprintf(e->mcastatus_msg,
					     "CA error on channel %d", chan);
				break;
			case 0x4:
				mce_snprintf(e->mcastatus_msg,
					     "Scrub error on channel %d", chan);
				break;
			}
		}
		decode_bitfield(e, status, memctrl_mc7);
		break;
	default:
		break;
	}

	/*
	 * Memory error specific code. Returns if the error is not a MC one
	 */

	/* Check if the error is at the memory controller */
	if ((mca >> 7) != 1)
		return;

	/* Ignore unless this is an corrected extended error from an iMC bank */
	if (e->bank < 7 || e->bank > 16 || (status & MCI_STATUS_UC) ||
	    !test_prefix(7, status & 0xefff))
		return;

	/*
	 * Parse the reported channel and ranks
	 */

	chan = EXTRACT(status, 0, 3);
	if (chan == 0xf) {
		mce_snprintf(e->mc_location, "memory_channel=unspecified");
	} else {
		chan = chan + 3 * (e->bank == 15);
		mce_snprintf(e->mc_location, "memory_channel=%d", chan);

		if (EXTRACT(e->misc, 62, 62))
			rank0 = EXTRACT(e->misc, 46, 50);
		if (EXTRACT(e->misc, 63, 63))
			rank1 = EXTRACT(e->misc, 51, 55);

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
}
