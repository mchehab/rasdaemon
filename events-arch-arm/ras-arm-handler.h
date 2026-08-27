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

void display_raw_data(struct trace_seq *s,
		      const uint8_t *buf,
		      uint32_t datalen);
struct ras_arm_event {
	char timestamp[64];
	int32_t error_count;
	int8_t affinity;
	int64_t mpidr;
	int64_t midr;
	int32_t running_state;
	int32_t psci_state;
	const uint8_t *pei_error;
	uint32_t pei_len;
	const uint8_t *ctx_error;
	uint32_t ctx_len;
	const uint8_t *vsei_error;
	uint32_t oem_len;
	char error_types[512];
	char error_flags[512];
	uint64_t error_info;
	uint64_t virt_fault_addr;
	uint64_t phy_fault_addr;
};

#endif
