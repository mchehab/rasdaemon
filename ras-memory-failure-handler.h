/* SPDX-License-Identifier: GPL-2.0-or-later */

/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2020. All rights reserved.
 */

#ifndef __RAS_MEMORY_FAILURE_HANDLER_H
#define __RAS_MEMORY_FAILURE_HANDLER_H

#include <traceevent/event-parse.h>

#include "ras-events.h"

extern unsigned long long poison_stat_threshold;

void mem_fail_event_trigger_setup(void);
int ras_memory_failure_event_handler(struct trace_seq *s,
				     struct tep_record *record,
				     struct tep_event *event, void *context);

#endif
