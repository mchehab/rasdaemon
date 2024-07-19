/* SPDX-License-Identifier: GPL-2.0-or-later */

/*
 * Copyright (C) 2019 Cong Wang <xiyou.wangcong@gmail.com>
 */

#ifndef __RAS_DISKERROR_HANDLER_H
#define __RAS_DISKERROR_HANDLER_H

#include <traceevent/event-parse.h>

#include "ras-events.h"

int ras_diskerror_event_handler(struct trace_seq *s,
				struct tep_record *record,
				struct tep_event *event, void *context);

#endif
