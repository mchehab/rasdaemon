/*
 * Copyright (C) 2013 Mauro Carvalho Chehab <mchehab+redhat@kernel.org>
 * Copyright (c) 2016, The Linux Foundation. All rights reserved.
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

struct ras_non_standard_event {
	char timestamp[64];
	const char *sec_type, *fru_id, *fru_text;
	const char *severity;
	const uint8_t *error;
	uint32_t length;
};

struct ras_arm_event {
	char timestamp[64];
	int32_t error_count;
	int8_t affinity;
	int64_t mpidr;
	int64_t midr;
	int32_t running_state;
	int32_t psci_state;
};

struct devlink_event {
	char timestamp[64];
	const char *bus_name;
	const char *dev_name;
	const char *driver_name;
	const char *reporter_name;
	char *msg;
};

struct diskerror_event {
	char timestamp[64];
	char *dev;
	unsigned long long sector;
	unsigned int nr_sector;
	const char *error;
	const char *rwbs;
	const char *cmd;
};

struct ras_mc_event;
struct ras_aer_event;
struct ras_extlog_event;
struct ras_non_standard_event;
struct ras_arm_event;
struct mce_event;
struct devlink_event;
struct diskerror_event;

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
#ifdef HAVE_NON_STANDARD
	sqlite3_stmt	*stmt_non_standard_record;
#endif
#ifdef HAVE_ARM
	sqlite3_stmt	*stmt_arm_record;
#endif
#ifdef HAVE_DEVLINK
	sqlite3_stmt	*stmt_devlink_event;
#endif
#ifdef HAVE_DISKERROR
	sqlite3_stmt	*stmt_diskerror_event;
#endif
};

struct db_fields {
	char *name;
	char *type;
};

struct db_table_descriptor {
	char                    *name;
	const struct db_fields  *fields;
	size_t                  num_fields;
};

int ras_mc_event_opendb(unsigned cpu, struct ras_events *ras);
int ras_mc_add_vendor_table(struct ras_events *ras, sqlite3_stmt **stmt,
			    const struct db_table_descriptor *db_tab);
int ras_store_mc_event(struct ras_events *ras, struct ras_mc_event *ev);
int ras_store_aer_event(struct ras_events *ras, struct ras_aer_event *ev);
int ras_store_mce_record(struct ras_events *ras, struct mce_event *ev);
int ras_store_extlog_mem_record(struct ras_events *ras, struct ras_extlog_event *ev);
int ras_store_non_standard_record(struct ras_events *ras, struct ras_non_standard_event *ev);
int ras_store_arm_record(struct ras_events *ras, struct ras_arm_event *ev);
int ras_store_devlink_event(struct ras_events *ras, struct devlink_event *ev);
int ras_store_diskerror_event(struct ras_events *ras, struct diskerror_event *ev);

#else
static inline int ras_mc_event_opendb(unsigned cpu, struct ras_events *ras) { return 0; };
static inline int ras_store_mc_event(struct ras_events *ras, struct ras_mc_event *ev) { return 0; };
static inline int ras_store_aer_event(struct ras_events *ras, struct ras_aer_event *ev) { return 0; };
static inline int ras_store_mce_record(struct ras_events *ras, struct mce_event *ev) { return 0; };
static inline int ras_store_extlog_mem_record(struct ras_events *ras, struct ras_extlog_event *ev) { return 0; };
static inline int ras_store_non_standard_record(struct ras_events *ras, struct ras_non_standard_event *ev) { return 0; };
static inline int ras_store_arm_record(struct ras_events *ras, struct ras_arm_event *ev) { return 0; };
static inline int ras_store_devlink_event(struct ras_events *ras, struct devlink_event *ev) { return 0; };
static inline int ras_store_diskerror_event(struct ras_events *ras, struct diskerror_event *ev) { return 0; };

#endif

#endif
