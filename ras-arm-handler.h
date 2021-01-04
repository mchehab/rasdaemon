/*
 * Copyright (c) 2016, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __RAS_ARM_HANDLER_H
#define __RAS_ARM_HANDLER_H

#include "ras-events.h"
#include "libtrace/event-parse.h"

int ras_arm_event_handler(struct trace_seq *s,
			 struct pevent_record *record,
			 struct event_format *event, void *context);
void display_raw_data(struct trace_seq *s,
		const uint8_t *buf,
		uint32_t datalen);
#endif
