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

#include "ras-mce-handler.h"

#define MCE_THERMAL_BANK	(MCE_EXTENDED_BANK + 0)
#define MCE_TIMEOUT_BANK        (MCE_EXTENDED_BANK + 90)

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

	return 0;
}

