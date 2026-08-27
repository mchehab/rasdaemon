/* SPDX-License-Identifier: GPL-2.0-or-later */

/*
 * Copyright (C) 2013 Mauro Carvalho Chehab <mchehab+huawei@kernel.org>
 */

#ifndef __RAS_MC_HANDLER_H
#define __RAS_MC_HANDLER_H

#include <traceevent/event-parse.h>

#include "core/ras-events.h"

extern unsigned long long mc_ce_stat_threshold;

void mc_event_trigger_setup(void);

int ras_mc_event_handler(struct trace_seq *s,
			 struct tep_record *record,
			 struct tep_event *event, void *context);

struct ras_mc_event {
	char timestamp[64];
	int error_count;
	const char *error_type, *msg, *label;
	unsigned char mc_index;
	signed char top_layer, middle_layer, lower_layer;
	unsigned long long address, grain, syndrome;
	const char *driver_detail;
	int erst;
};

#ifdef HAVE_DB
int ras_mc_event_opendb(unsigned int cpu, struct ras_events *ras);
int ras_mc_event_closedb(unsigned int cpu, struct ras_events *ras);
int db_mc_event(struct ras_events *ras, struct ras_mc_event *ev);
#else
static inline int ras_mc_event_opendb(unsigned int cpu,
				      struct ras_events *ras) { return 0; }
static inline int ras_mc_event_closedb(unsigned int cpu,
				       struct ras_events *ras) { return 0; }
static inline int db_mc_event(struct ras_events *ras,
			      struct ras_mc_event *ev) { return 0; }
#endif


#endif
