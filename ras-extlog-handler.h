/* SPDX-License-Identifier: GPL-2.0-or-later */

/*
 * Copyright (C) 2014 Tony Luck <tony.luck@intel.com>
 */

#ifndef __RAS_EXTLOG_HANDLER_H
#define __RAS_EXTLOG_HANDLER_H

#include <stdint.h>
#include <traceevent/event-parse.h>

#include "ras-events.h"

int ras_extlog_mem_event_handler(struct trace_seq *s,
				 struct tep_record *record,
				 struct tep_event *event,
				 void *context);
#endif
