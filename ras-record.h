/*
 * Copyright (C) 2013 Mauro Carvalho Chehab <mchehab@redhat.com>
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

#ifndef __RAS_RECORD_H
#define __RAS_RECORD_H

#include "config.h"

struct ras_events *ras;

struct ras_mc_event {
	char timestamp[64];
	int error_count;
	const char *error_type, *msg, *label;
	unsigned char mc_index;
	signed char top_layer, middle_layer, lower_layer;
	unsigned long long address, grain, syndrome;
	const char *driver_detail;
};

#ifdef HAVE_SQLITE3

#include <sqlite3.h>

struct sqlite3_priv {
	sqlite3		*db;
	sqlite3_stmt	*stmt;
};

int ras_mc_event_opendb(unsigned cpu, struct ras_events *ras);
int ras_store_mc_event(struct ras_events *ras, struct ras_mc_event *ev);

#else
static inline int ras_mc_event_opendb(unsigned cpu, struct ras_events *ras) { return 0; };
static inline int ras_store_mc_event(struct ras_events *ras, struct ras_mc_event *ev) { return 0; };
#endif

#endif
