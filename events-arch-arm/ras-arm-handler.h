/* SPDX-License-Identifier: GPL-2.0-only */

/*
 * Copyright (c) 2016, The Linux Foundation. All rights reserved.
 */

#ifndef __RAS_ARM_HANDLER_H
#define __RAS_ARM_HANDLER_H

#include <traceevent/event-parse.h>

#include "core/ras-events.h"

struct ras_arm_event;

/*
 * ARM Processor Error Information Structure, According to
 * UEFI_2_9 specification chapter N2.4.4.
 */
#pragma pack(1)
struct ras_arm_err_info {
	uint8_t version;
	uint8_t length;
	uint16_t validation_bits;
	uint8_t type;
	uint16_t multiple_error;
	uint8_t flags;
	uint64_t error_info;
	uint64_t virt_fault_addr;
	uint64_t physical_fault_addr;
};

#pragma pack()

int ras_arm_event_handler(struct trace_seq *s,
			  struct tep_record *record,
			  struct tep_event *event, void *context);
void display_raw_data(struct trace_seq *s,
		      const uint8_t *buf,
		      uint32_t datalen);
#ifdef HAVE_UNITTEST
int ras_arm_test_parse_processor(struct trace_seq *s,
				 struct ras_arm_event *event);
#ifdef HAVE_CPU_FAULT_ISOLATION
int ras_arm_test_count_errors(struct ras_arm_event *event, int severity);
#endif
#endif
#endif
