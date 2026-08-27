/* SPDX-License-Identifier: GPL-2.0-or-later */

/*
 * Copyright (C) 2014 Tony Luck <tony.luck@intel.com>
 */

#ifndef __RAS_EXTLOG_HANDLER_H
#define __RAS_EXTLOG_HANDLER_H

#include <stdint.h>
#include <traceevent/event-parse.h>

#include "core/ras-events.h"

int ras_extlog_mem_event_handler(struct trace_seq *s,
				 struct tep_record *record,
				 struct tep_event *event,
				 void *context);

#ifdef HAVE_UNITTEST
const char *ras_extlog_test_error_type(int type);
const char *ras_extlog_test_severity(int severity);
unsigned long long ras_extlog_test_mask(int lsb);
#endif
struct ras_extlog_event {
	char timestamp[64];
	int32_t error_seq;
	int8_t etype;
	int8_t severity;
	unsigned long long address;
	int8_t pa_mask_lsb;
	const char *fru_id;
	const char *fru_text;
	const char *cper_data;
	unsigned short cper_data_length;
};


#endif
