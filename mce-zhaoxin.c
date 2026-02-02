// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2026, Zhaoxin, Inc. All rights reserved.
 * Author: Lyle Li <LyleLi-oc@zhaoxin.com>
 */

#include <stdio.h>
#include <string.h>

#include "bitfield.h"
#include "ras-mce-handler.h"

#define MCG_TES_P	BIT_ULL(11)	/* Yellow bit cache threshold supported */

static void decode_mcg(struct mce_event *e)
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
	if (mcgstatus & MCG_STATUS_LMCE)
		mce_snprintf(e->mcgstatus_msg, "LMCE");
}

static void decode_mci(struct mce_event *e)
{
	uint64_t track = 0;

	if (e->status & MCI_STATUS_UC) {
		if (e->status & MCI_STATUS_PCC) {
			mce_snprintf(e->error_msg, "Uncorrected Fatal Error");
		} else {
			switch (e->status & (MCI_STATUS_S | MCI_STATUS_AR)) {
			case 0:
				mce_snprintf(e->error_msg, "Uncorrected No Action Required Error");
				break;
			case MCI_STATUS_S:
				mce_snprintf(e->error_msg, "Uncorrected Software Recoverable Action Optional Error");
			case MCI_STATUS_S | MCI_STATUS_AR:
				mce_snprintf(e->error_msg, "Uncorrected Software Recoverable Action Required Error");
				break;
			default:
				break;
			}
		}
	} else {
		mce_snprintf(e->error_msg, "Corrected Error.");
	}

	if (!(e->status & MCI_STATUS_VAL))
		mce_snprintf(e->mcistatus_msg, "MCE_INVALID");

	if (e->status & MCI_STATUS_OVER)
		mce_snprintf(e->mcistatus_msg, "Error_overflow");

	if (e->status & MCI_STATUS_EN)
		mce_snprintf(e->mcistatus_msg, "Error_enabled");

	if (e->status & MCI_STATUS_PCC)
		mce_snprintf(e->mcistatus_msg, "Processor_context_corrupt");

	if (e->mcgcap & MCG_TES_P && !(e->status & MCI_STATUS_UC)) {
		track = (e->status >> 53) & 3;
		if (track == 1)
			mce_snprintf(e->mcistatus_msg, "Threshold based error status: Green");
		else if (track == 2)
			mce_snprintf(e->mcistatus_msg, "Threshold based error status: Yellow");
	}
}

int parse_zhaoxin_event(struct ras_events *ras, struct mce_event *e)
{
	struct mce_priv *mce = ras->mce_priv;

	decode_mcg(e);
	decode_mci(e);

	switch (mce->cputype) {
	case CPU_ZHAOXIN_KH50000:
		kh50000_decode_model(e);
		break;
	default:
		break;
	}

	return 0;
}
