/*
 * Copyright (C) 2013 Mauro Carvalho Chehab <mchehab@redhat.com>
 *
 * The code below were adapted from Andi Kleen/Intel/SuSe mcelog code,
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

#define MCE_THERMAL_BANK	(MCE_EXTENDED_BANK + 0)
#define MCE_TIMEOUT_BANK        (MCE_EXTENDED_BANK + 90)

static decode_termal_bank(struct mce_event *e)
{
	if (e->status & 1) {
		mce_snprintf(e->mcgstatus_msg, "Processor %d heated above trip temperature. Throttling enabled.", e->cpu);
		mce_snprintf(e->user_action, "Please check your system cooling. Performance will be impacted");
	} else {
		sprintf(e->error_msg, "Processor %d below trip temperature. Throttling disabled", e->cpu);
	}
}

static void decode_mcg(struct mce_event *e)
{
	int n, len = sizeof(e->mcgstatus_msg);
	uint64_t mcgstatus = e->mcgstatus;
	char *p = e->mcgstatus_msg;

	mce_snprintf(e->mcgstatus_msg, "mcgstatus= %d", e->mcgstatus);

	if (mcgstatus & MCG_STATUS_RIPV)
		mce_snprintf(e->mcgstatus_msg, "RIPV");
	if (mcgstatus & MCG_STATUS_EIPV)
		mce_snprintf(e->mcgstatus_msg, "EIPV");
	if (mcgstatus & MCG_STATUS_MCIP)
		mce_snprintf(e->mcgstatus_msg, "MCIP");
}

static void bank_name(struct mce_event *e)
{
	char *buf = e->bank_name;

	switch (e->bank) {
	case MCE_THERMAL_BANK:
		strcpy(buf, "THERMAL EVENT");
		break;
	case MCE_TIMEOUT_BANK:
		strcpy(buf, "Timeout waiting for exception on other CPUs");
		break;
	default:
		break;
	}
}

int parse_intel_event(struct ras_events *ras, struct mce_event *e)
{
	bank_name(e);

	if (e->bank == MCE_THERMAL_BANK) {
		decode_termal_bank(e);
		return 0;
	}
	decode_mcg(e);

	return 0;
}

