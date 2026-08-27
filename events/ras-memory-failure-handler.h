/* SPDX-License-Identifier: GPL-2.0-or-later */

/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2020. All rights reserved.
 */

#ifndef __RAS_MEMORY_FAILURE_HANDLER_H
#define __RAS_MEMORY_FAILURE_HANDLER_H

#include <traceevent/event-parse.h>

#include "core/ras-events.h"

extern unsigned long long poison_stat_threshold;

void mem_fail_event_trigger_setup(void);
int ras_memory_failure_event_handler(struct trace_seq *s,
				     struct tep_record *record,
				     struct tep_event *event, void *context);

#ifdef HAVE_UNITTEST
const char *ras_memory_failure_test_page_type(int page_type);
const char *ras_memory_failure_test_action_result(int result);
#endif

struct ras_mf_event {
	char timestamp[64];
	char pfn[30];
	const char *page_type;
	const char *action_result;
};

int db_mf_event(struct ras_events *ras, struct ras_mf_event *ev);


#endif
