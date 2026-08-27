/* SPDX-License-Identifier: GPL-2.0-or-later */

/*
 * Copyright (C) 2013 Mauro Carvalho Chehab <mchehab+huawei@kernel.org>
 */

#ifndef __RAS_EVENTS_H
#define __RAS_EVENTS_H

#include <pthread.h>
#include <stdbool.h>
#include <time.h>
#include <traceevent/event-parse.h>

#include "core/modules.h"
#include "core/types.h"

extern char *choices_disable;
extern long user_hz;

struct mce_priv;
struct ras_mc_offline_event;

enum {
	MC_EVENT,
	MCE_EVENT,
	AER_EVENT,
	NON_STANDARD_EVENT,
	ARM_EVENT,
	EXTLOG_EVENT,
	DEVLINK_EVENT,
	DISKERROR_EVENT,
	MF_EVENT,
	SIGNAL_EVENT,
	CXL_POISON_EVENT,
	CXL_AER_UE_EVENT,
	CXL_AER_CE_EVENT,
	CXL_OVERFLOW_EVENT,
	CXL_GENERIC_EVENT,
	CXL_GENERAL_MEDIA_EVENT,
	CXL_DRAM_EVENT,
	CXL_MEMORY_MODULE_EVENT,
	CXL_MEMORY_SPARING_EVENT,
	RERI_EVENT,
	NR_EVENTS
};

struct ras_db;

struct ras_event_entry {
	const char *group;
	const char *event;
	tep_event_handler_func handler;
	const char *filter;
	int id;
	bool trigger;
#ifdef HAVE_UNITTEST
	enum test_group test_group;
	int (*test)(void);
	unsigned int test_priority;
#endif
};

struct ras_events {
	char			tracing[MAX_PATH + 1];
	struct tep_handle	*pevent;
	int			page_size;

	/* Booleans */
	unsigned		use_uptime: 1;
	unsigned		record_events: 1;

	/* For timestamp */
	time_t			uptime_diff;

	/* For ras-record and ras-db */
	struct ras_db		*db;
	void			*db_priv;
	int			db_ref_count;
	unsigned int		num_events;
	pthread_mutex_t		db_lock;

	/* For the mce handler */
	struct mce_priv		*mce_priv;

	/* For ABRT socket*/
	int			socketfd;
	int			daemon_active_fd;

	struct tep_event_filter	*filters[NR_EVENTS];
};

struct pthread_data {
	pthread_t		thread;
	struct tep_handle	*pevent;
	struct ras_events	*ras;
	int			cpu;
};

/* Should match the code at Kernel's include/linux/edac.c */
enum hw_event_mc_err_type {
	HW_EVENT_ERR_CORRECTED,
	HW_EVENT_ERR_UNCORRECTED,
	HW_EVENT_ERR_DEFERRED,
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
int ras_offline_mce_event(struct ras_mc_offline_event *event);

int ras_event_register(const struct ras_event_entry *entry);
int ras_events_prepare(struct ras_events *ras, int record_events);
int handle_ras_events(struct ras_events *ras,
		      int record_events, int enable_ipmitool);
#ifdef HAVE_UNITTEST
bool ras_events_test_is_disabled(const char *group, const char *event);
#endif

#endif
