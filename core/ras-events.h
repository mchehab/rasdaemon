/* SPDX-License-Identifier: GPL-2.0-or-later */

/*
 * Copyright (C) 2013 Mauro Carvalho Chehab <mchehab+huawei@kernel.org>
 */

#ifndef __RAS_EVENTS_H
#define __RAS_EVENTS_H

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include <traceevent/event-parse.h>

#include "core/modules.h"
#include "core/types.h"

extern char *choices_disable;
extern long user_hz;

struct mce_priv;
struct ras_mc_offline_event;

/**
 * enum ras_event_id - decoded event payload types
 * @MC_EVENT: memory-controller event
 * @MCE_EVENT: x86 machine-check event
 * @AER_EVENT: PCIe AER event
 * @NON_STANDARD_EVENT: non-standard CPER event
 * @ARM_EVENT: Arm processor error event
 * @EXTLOG_EVENT: extended machine-check log event
 * @DEVLINK_EVENT: devlink health event
 * @DISKERROR_EVENT: block I/O error event
 * @MF_EVENT: memory-failure event
 * @SIGNAL_EVENT: fatal-signal event
 * @CXL_POISON_EVENT: CXL poison-list event
 * @CXL_AER_UE_EVENT: CXL uncorrectable AER event
 * @CXL_AER_CE_EVENT: CXL correctable AER event
 * @CXL_OVERFLOW_EVENT: CXL overflow event
 * @CXL_GENERIC_EVENT: generic CXL event
 * @CXL_GENERAL_MEDIA_EVENT: CXL general-media event
 * @CXL_DRAM_EVENT: CXL DRAM event
 * @CXL_MEMORY_MODULE_EVENT: CXL memory-module event
 * @CXL_MEMORY_SPARING_EVENT: CXL memory-sparing event
 * @RERI_EVENT: RISC-V RERI event
 * @NR_EVENTS: number of event identifiers
 */
enum ras_event_id {
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

/**
 * typedef record_function - persist one decoded event
 * @ras: event-loop and database context
 * @event: concrete payload selected by the event descriptor
 *
 * Return:
 * 0 on success or the recorder/backend-specific nonzero error on failure.
 */
typedef int (*record_function)(struct ras_events *ras, void *event);

/**
 * enum ras_event_consumer_priority - event-consumer delivery order
 * @PRI_CPU_ISOLATION: CPU offlining/isolation actions
 * @PRI_MEM_ISOLATION: page and row isolation actions
 * @PRI_POISON_PAGE: poison-page accounting
 * @PRI_PLATFORM_ACTION: platform-specific actions
 * @PRI_REPORTING: external reporting
 * @PRI_DB_RECORD: database persistence
 * @PRI_NORMAL: consumers without ordering constraints
 */
enum ras_event_consumer_priority {
	PRI_CPU_ISOLATION = 10,
	PRI_MEM_ISOLATION = 20,
	PRI_POISON_PAGE = 30,
	PRI_PLATFORM_ACTION = 40,
	PRI_REPORTING = 50,
	PRI_DB_RECORD = 60,
	PRI_NORMAL = 100,
};

/**
 * struct ras_event_consumer - immutable decoded-event consumer
 * @name: unique diagnostic name and equal-priority ordering key
 * @priority: delivery priority
 * @events: bitmap of accepted enum ras_event_id values
 * @consume: synchronous callback; the publisher retains payload ownership
 */
struct ras_event_consumer {
	const char *name;
	enum ras_event_consumer_priority priority;
	uint64_t events;
	int (*consume)(struct ras_events *ras, int event, void *data);
};

/**
 * struct ras_event_entry - immutable trace-event registration descriptor
 * @group: trace-event subsystem name
 * @event: trace-event name
 * @handler: libtraceevent callback
 * @filter: fixed kernel filter string, or NULL
 * @filter_cb: optional callback producing a kernel filter string
 * @prepare: optional per-event preparation callback
 * @enabled: optional callback after successful event enablement
 * @trigger_setup: optional trace-trigger configuration callback
 * @id: decoded enum ras_event_id
 * @order: ascending handler registration order
 * @record: optional database recorder
 * @test_group: unit-test family when unit tests are enabled
 * @test: optional unit-test callback
 * @test_priority: ascending test execution order
 *
 * Descriptors have static lifetime. Callback resources are owned by their
 * module and must remain valid until ras_events_cleanup().
 */
struct ras_event_entry {
	const char *group;
	const char *event;
	tep_event_handler_func handler;
	const char *filter;
	const char *(*filter_cb)(struct ras_events *ras);
	int (*prepare)(struct ras_events *ras);
	void (*enabled)(struct ras_events *ras);
	void (*trigger_setup)(void);
	int id;
	int order;
	record_function record;
#ifdef HAVE_UNITTEST
	enum test_group test_group;
	int (*test)(void);
	unsigned int test_priority;
#endif
};

/**
 * struct ras_events - process-wide tracing and database state
 * @tracing: mounted tracefs/debugfs tracing directory
 * @pevent: shared trace-event parser
 * @page_size: kernel tracing page size
 * @use_uptime: timestamps use uptime rather than wall clock
 * @record_events: database recording is requested
 * @uptime_diff: wall-clock offset from monotonic uptime
 * @db: active backend connection
 * @db_priv: backend-private per-session data
 * @db_ref_count: number of matching db_open() references
 * @num_events: number of decoded events
 * @db_lock: serializes legacy per-CPU database recorder access
 * @mce_priv: x86 MCE decoder state
 * @socketfd: ABRT reporting socket
 * @daemon_active_fd: lock file descriptor proving daemon ownership
 * @filters: installed filters indexed by enum ras_event_id
 */
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

/**
 * struct pthread_data - state for one legacy per-CPU reader thread
 * @thread: POSIX thread identifier
 * @pevent: thread-local event parser
 * @ras: shared process context
 * @cpu: logical CPU read by this thread
 */
struct pthread_data {
	pthread_t		thread;
	struct tep_handle	*pevent;
	struct ras_events	*ras;
	int			cpu;
};

/* NOTE: Should match the code at Kernel's include/linux/edac.c */
/**
 * enum hw_event_mc_err_type - memory-controller error classifications
 * @HW_EVENT_ERR_CORRECTED: corrected error
 * @HW_EVENT_ERR_UNCORRECTED: uncorrected non-fatal error
 * @HW_EVENT_ERR_DEFERRED: deferred error
 * @HW_EVENT_ERR_FATAL: fatal error
 * @HW_EVENT_ERR_INFO: informational event
 */
enum hw_event_mc_err_type {
	HW_EVENT_ERR_CORRECTED,
	HW_EVENT_ERR_UNCORRECTED,
	HW_EVENT_ERR_DEFERRED,
	HW_EVENT_ERR_FATAL,
	HW_EVENT_ERR_INFO,
};

/* NOTE: Should match the code at Kernel's /drivers/pci/pcie/aer/aerdrv_errprint.c */
/**
 * enum hw_event_aer_err_type - PCIe AER classifications
 * @HW_EVENT_AER_UNCORRECTED_NON_FATAL: uncorrectable non-fatal event
 * @HW_EVENT_AER_UNCORRECTED_FATAL: uncorrectable fatal event
 * @HW_EVENT_AER_CORRECTED: corrected event
 */
enum hw_event_aer_err_type {
	HW_EVENT_AER_UNCORRECTED_NON_FATAL,
	HW_EVENT_AER_UNCORRECTED_FATAL,
	HW_EVENT_AER_CORRECTED,
};

/* NOTE: Should match the code at Kernel's include/acpi/ghes.h */
/**
 * enum ghes_severity - ACPI GHES severity values
 * @GHES_SEV_NO: no severity
 * @GHES_SEV_CORRECTED: corrected error
 * @GHES_SEV_RECOVERABLE: recoverable error
 * @GHES_SEV_PANIC: fatal error
 */
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
int ras_event_record(struct ras_events *ras, int event, void *data);

/*
 * Register a static consumer descriptor. Consumers run by ascending priority,
 * then name. Duplicate names are rejected.
 */
int ras_event_consumer_register(const struct ras_event_consumer *consumer);
int ras_event_consumer_unregister(const struct ras_event_consumer *consumer);

/*
 * Synchronously publish decoded event data to each interested consumer.
 * Delivery continues after errors and the first consumer error is returned.
 * The data remains owned by the publisher and is valid only during this call.
 */
int ras_event_publish(struct ras_events *ras, int event, void *data);
int ras_event_filter(struct ras_events *ras, const char *group,
		     const char *event, const char *filter);
int ras_events_prepare(struct ras_events *ras, int record_events);
void ras_events_cleanup(struct ras_events *ras);
int handle_ras_events(struct ras_events *ras);
#ifdef HAVE_UNITTEST
bool ras_events_test_is_disabled(const char *group, const char *event);
#endif

#define REGISTER_RAS_EVENT(entry) \
	static void __attribute__((constructor)) register_##entry(void) \
	{ \
		ras_event_register(&(entry)); \
	}

#define REGISTER_RAS_EVENT_CONSUMER(consumer) \
	static void __attribute__((constructor)) register_##consumer(void) \
	{ \
		ras_event_consumer_register(&(consumer)); \
	}

#endif
