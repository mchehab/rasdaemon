/* SPDX-License-Identifier: GPL-2.0-or-later */

/*
 * Copyright (C) 2013 Mauro Carvalho Chehab <mchehab+huawei@kernel.org>
 */

#ifndef __RAS_AER_HANDLER_H
#define __RAS_AER_HANDLER_H

#include <traceevent/event-parse.h>

#include "ras-events.h"

void aer_event_trigger_setup(void);
int ras_aer_event_handler(struct trace_seq *s,
			  struct tep_record *record,
			  struct tep_event *event, void *context);

void ras_aer_handler_init(int enable_ipmitool);
#endif
