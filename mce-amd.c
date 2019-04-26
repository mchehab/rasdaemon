/*
 * Copyright (c) 2018, The AMD, Inc. All rights reserved.
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

/* Error Code Types */
#define TLB_ERROR(x)                    (((x) & 0xFFF0) == 0x0010)
#define MEM_ERROR(x)                    (((x) & 0xFF00) == 0x0100)
#define BUS_ERROR(x)                    (((x) & 0xF800) == 0x0800)
#define INT_ERROR(x)                    (((x) & 0xF4FF) == 0x0400)

/* Error code: transaction type (TT) */
static char *transaction[] = {
	"instruction", "data", "generic", "reserved"
};
/* Error codes: cache level (LL) */
static char *cachelevel[] = {
	"reserved", "L1", "L2", "L3/generic"
};
/* Error codes: memory transaction type (RRRR) */
static char *memtrans[] = {
	"generic", "generic read", "generic write", "data read",
	"data write", "instruction fetch", "prefetch", "evict", "snoop",
	"?", "?", "?", "?", "?", "?", "?"
};
/* Participation Processor */
static char *partproc[] = {
	"local node origin", "local node response",
	"local node observed", "generic participation"
};
/* Timeout */
static char *timeout[] = {
	"request didn't time out",
	"request timed out"
};
/* internal unclassified error code */
static char *internal[] = { "reserved",
			    "reserved",
			    "hardware assert",
			    "reserved" };

#define TT(x)         (((x) >> 2) & 0x3)   /*bit 2, bit 3*/
#define TT_MSG(x)     transaction[TT(x)]
#define LL(x)         ((x) & 0x3)          /*bit 0, bit 1*/
#define LL_MSG(x)     cachelevel[LL(x)]

#define R4(x)         (((x) >> 4) & 0xF)   /*bit 4, bit 5, bit 6, bit 7 */
#define R4_MSG(x)     ((R4(x) < 9) ?  memtrans[R4(x)] : "Wrong R4!")

#define TO(x)         (((x) >> 8) & 0x1)   /*bit 8*/
#define TO_MSG(x)     timeout[TO(x)]
#define PP(x)         (((x) >> 9) & 0x3)   /*bit 9, bit 10*/
#define PP_MSG(x)     partproc[PP(x)]

#define UU(x)         (((x) >> 8) & 0x3)   /*bit 8, bit 9*/
#define UU_MSG(x)     internal[UU(x)]

void decode_amd_errcode(struct mce_event *e)
{
	uint16_t ec = e->status & 0xffff;
	uint16_t ecc = (e->status >> 45) & 0x3;

	if (e->status & MCI_STATUS_UC) {
		if (e->status & MCI_STATUS_PCC)
			strcpy(e->error_msg, "System Fatal error.");
		if (e->mcgstatus & MCG_STATUS_RIPV)
			strcpy(e->error_msg,
			       "Uncorrected, software restartable error.");
		strcpy(e->error_msg,
		       "Uncorrected, software containable error.");
	} else if (e->status & MCI_STATUS_DEFERRED)
		strcpy(e->error_msg, "Deferred error, no action required.");
	else
		strcpy(e->error_msg, "Corrected error, no action required.");

	if (!(e->status & MCI_STATUS_VAL))
		mce_snprintf(e->mcistatus_msg, "MCE_INVALID");

	if (e->status & MCI_STATUS_OVER)
		mce_snprintf(e->mcistatus_msg, "Error_overflow");

	if (e->status & MCI_STATUS_PCC)
		mce_snprintf(e->mcistatus_msg, "Processor_context_corrupt");

	if (ecc)
		mce_snprintf(e->mcistatus_msg,
			     "%sECC", ((ecc == 2) ? "C" : "U"));

	if (INT_ERROR(ec)) {
		mce_snprintf(e->mcastatus_msg, "Internal '%s'", UU_MSG(ec));
		return;
	}

	if (TLB_ERROR(ec))
		mce_snprintf(e->mcastatus_msg,
			     "TLB Error 'tx: %s, level: %s'",
			     TT_MSG(ec), LL_MSG(ec));
	else if (MEM_ERROR(ec))
		mce_snprintf(e->mcastatus_msg,
			     "Memory Error 'mem-tx: %s, tx: %s, level: %s'",
			     R4_MSG(ec), TT_MSG(ec), LL_MSG(ec));
	else if (BUS_ERROR(ec))
		mce_snprintf(e->mcastatus_msg,
			     "Bus Error '%s, %s, mem-tx: %s, level: %s'",
			     PP_MSG(ec), TO_MSG(ec),
			     R4_MSG(ec), LL_MSG(ec));
	return;

}
