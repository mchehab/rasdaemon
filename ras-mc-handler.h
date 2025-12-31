/* SPDX-License-Identifier: GPL-2.0-or-later */

/*
 * Copyright (C) 2013 Mauro Carvalho Chehab <mchehab+huawei@kernel.org>
 */

#ifndef __RAS_MC_HANDLER_H
#define __RAS_MC_HANDLER_H

#include <traceevent/event-parse.h>

#include "ras-events.h"
extern unsigned long long mc_ce_stat_threshold;

void mc_event_trigger_setup(void);

int ras_mc_event_handler(struct trace_seq *s,
			 struct tep_record *record,
			 struct tep_event *event, void *context);

#endif
