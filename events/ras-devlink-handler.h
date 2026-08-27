/* SPDX-License-Identifier: GPL-2.0-or-later */

/*
 * Copyright (C) 2019 Cong Wang <xiyou.wangcong@gmail.com>
 */

#ifndef __RAS_DEVLINK_HANDLER_H
#define __RAS_DEVLINK_HANDLER_H

#include <traceevent/event-parse.h>

#include "core/ras-events.h"

struct devlink_event {
	char timestamp[64];
	const char *bus_name;
	const char *dev_name;
	const char *driver_name;
	const char *reporter_name;
	char *msg;
};

#endif
