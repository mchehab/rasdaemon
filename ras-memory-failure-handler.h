/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2020. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
*/

#ifndef __RAS_MEMORY_FAILURE_HANDLER_H
#define __RAS_MEMORY_FAILURE_HANDLER_H

#include "ras-events.h"
#include <traceevent/event-parse.h>

void mem_fail_event_trigger_setup(void);
int ras_memory_failure_event_handler(struct trace_seq *s,
				     struct tep_record *record,
				     struct tep_event *event, void *context);

#endif
