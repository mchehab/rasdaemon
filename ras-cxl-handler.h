/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
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

#ifndef __RAS_CXL_HANDLER_H
#define __RAS_CXL_HANDLER_H

#include "ras-events.h"
#include <traceevent/event-parse.h>

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
