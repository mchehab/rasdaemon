/*
 * Copyright (C) 2013 Mauro Carvalho Chehab <mchehab+redhat@kernel.org>
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

#ifndef __RAS_EVENTS_H
#define __RAS_EVENTS_H

#include "ras-record.h"

#include <pthread.h>
#include <time.h>

#define MAX_PATH 1024
#define STR(x) #x

struct mce_priv;

enum {
	MC_EVENT,
	MCE_EVENT,
	AER_EVENT,
	NON_STANDARD_EVENT,
	ARM_EVENT,
	EXTLOG_EVENT,
	DEVLINK_EVENT,
	DISKERROR_EVENT,
	NR_EVENTS
};

struct ras_events {
	char debugfs[MAX_PATH + 1];
	char tracing[MAX_PATH + 1];
	struct pevent	*pevent;
	int		page_size;

	/* Booleans */
	unsigned	use_uptime: 1;
	unsigned        record_events: 1;

	/* For timestamp */
	time_t		uptime_diff;

	/* For ras-record */
	void		*db_priv;

	/* For the mce handler */
	struct mce_priv	*mce_priv;

	/* For ABRT socket*/
	int socketfd;

	struct event_filter *filters[NR_EVENTS];
};

struct pthread_data {
	pthread_t		thread;
	struct pevent		*pevent;
	struct ras_events	*ras;
	int			cpu;
};


/* Should match the code at Kernel's include/linux/edac.c */
enum hw_event_mc_err_type {
	HW_EVENT_ERR_CORRECTED,
	HW_EVENT_ERR_UNCORRECTED,
	HW_EVENT_ERR_FATAL,
	HW_EVENT_ERR_INFO,
};

/* Should match the code at Kernel's /drivers/pci/pcie/aer/aerdrv_errprint.c */
enum hw_event_aer_err_type {
	HW_EVENT_AER_UNCORRECTED_NON_FATAL,
	HW_EVENT_AER_UNCORRECTED_FATAL,
	HW_EVENT_AER_CORRECTED,
};

/* Should match the code at Kernel's include/acpi/ghes.h */
enum ghes_severity {
	GHES_SEV_NO,
	GHES_SEV_CORRECTED,
	GHES_SEV_RECOVERABLE,
	GHES_SEV_PANIC,
};

/* Function prototypes */
int toggle_ras_mc_event(int enable);
int handle_ras_events(int record_events);

#endif
