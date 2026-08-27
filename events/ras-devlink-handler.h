/* SPDX-License-Identifier: GPL-2.0-or-later */

/*
 * Copyright (C) 2019 Cong Wang <xiyou.wangcong@gmail.com>
 */

#ifndef __RAS_DEVLINK_HANDLER_H
#define __RAS_DEVLINK_HANDLER_H

#include <traceevent/event-parse.h>

#include "core/ras-events.h"

int ras_net_xmit_timeout_handler(struct trace_seq *s,
				 struct tep_record *record,
				 struct tep_event *event, void *context);

int ras_devlink_event_handler(struct trace_seq *s,
			      struct tep_record *record,
			      struct tep_event *event, void *context);

struct devlink_event {
	char timestamp[64];
	const char *bus_name;
	const char *dev_name;
	const char *driver_name;
	const char *reporter_name;
	char *msg;
};

int db_devlink_event(struct ras_events *ras, struct devlink_event *ev);


#endif
