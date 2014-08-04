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

#include <stdint.h>
#include "config.h"

extern long user_hz;

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

struct ras_aer_event {
	char timestamp[64];
	const char *error_type;
	const char *dev_name;
	const char *msg;
};

struct ras_extlog_event {
	char timestamp[64];
	int32_t error_seq;
	int8_t etype;
	int8_t severity;
	unsigned long long address;
	int8_t pa_mask_lsb;
	const char *fru_id;
	const char *fru_text;
	const char *cper_data;
	unsigned short cper_data_length;
};

struct ras_mc_event;
struct ras_aer_event;
struct ras_extlog_event;
struct mce_event;

#ifdef HAVE_SQLITE3

#include <sqlite3.h>

struct sqlite3_priv {
	sqlite3		*db;
	sqlite3_stmt	*stmt_mc_event;
#ifdef HAVE_AER
	sqlite3_stmt	*stmt_aer_event;
#endif
#ifdef HAVE_MCE
	sqlite3_stmt	*stmt_mce_record;
#endif
#ifdef HAVE_EXTLOG
	sqlite3_stmt	*stmt_extlog_record;
#endif
};

int ras_mc_event_opendb(unsigned cpu, struct ras_events *ras);
int ras_store_mc_event(struct ras_events *ras, struct ras_mc_event *ev);
int ras_store_aer_event(struct ras_events *ras, struct ras_aer_event *ev);
int ras_store_mce_record(struct ras_events *ras, struct mce_event *ev);
int ras_store_extlog_mem_record(struct ras_events *ras, struct ras_extlog_event *ev);

#else
static inline int ras_mc_event_opendb(unsigned cpu, struct ras_events *ras) { return 0; };
static inline int ras_store_mc_event(struct ras_events *ras, struct ras_mc_event *ev) { return 0; };
static inline int ras_store_aer_event(struct ras_events *ras, struct ras_aer_event *ev) { return 0; };
static inline int ras_store_mce_record(struct ras_events *ras, struct mce_event *ev) { return 0; };
static inline int ras_store_extlog_mem_record(struct ras_events *ras, struct ras_extlog_event *ev) { return 0; };

#endif

#endif
