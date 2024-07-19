/* SPDX-License-Identifier: GPL-2.0-or-later */

/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 */

#ifndef __RAS_CXL_HANDLER_H
#define __RAS_CXL_HANDLER_H

#include <traceevent/event-parse.h>

#include "ras-events.h"

int ras_cxl_poison_event_handler(struct trace_seq *s,
				 struct tep_record *record,
				 struct tep_event *event, void *context);

int ras_cxl_aer_ue_event_handler(struct trace_seq *s,
				 struct tep_record *record,
				 struct tep_event *event, void *context);

int ras_cxl_aer_ce_event_handler(struct trace_seq *s,
				 struct tep_record *record,
				 struct tep_event *event, void *context);
int ras_cxl_overflow_event_handler(struct trace_seq *s,
				   struct tep_record *record,
				   struct tep_event *event, void *context);
int ras_cxl_generic_event_handler(struct trace_seq *s,
				  struct tep_record *record,
				  struct tep_event *event, void *context);
int ras_cxl_general_media_event_handler(struct trace_seq *s,
					struct tep_record *record,
					struct tep_event *event, void *context);
int ras_cxl_dram_event_handler(struct trace_seq *s,
			       struct tep_record *record,
			       struct tep_event *event, void *context);
int ras_cxl_memory_module_event_handler(struct trace_seq *s,
					struct tep_record *record,
					struct tep_event *event, void *context);
#endif
