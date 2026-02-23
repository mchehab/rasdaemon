/* SPDX-License-Identifier: GPL-2.0-only */

/*
 * Copyright (c) 2026, NVIDIA CORPORATION & AFFILIATES.  All rights reserved.
 */

#ifndef __NON_STANDARD_NVIDIA_H
#define __NON_STANDARD_NVIDIA_H

#include <traceevent/event-parse.h>

#include "types.h"

struct ras_ns_ev_decoder;

#define NVIDIA_SEC_TYPE_UUID "6d5244f2-2712-11ec-bea7-cb3fdb95c786"

/* NVIDIA CPER Error Section Structure */
struct nvidia_cper_sec {
	char     signature[16];
	uint16_t error_type;
	uint16_t error_instance;
	uint8_t  severity;
	uint8_t  socket;
	uint8_t  number_regs;
	uint8_t  reserved;
	uint64_t instance_base;
	/* Register pairs (address, value) follow */
} __attribute__((packed));

/* Register pair structure */
struct nvidia_reg_pair {
	uint64_t address;
	uint64_t value;
} __attribute__((packed));

enum {
	NVIDIA_FIELD_SIGNATURE,
	NVIDIA_FIELD_ERROR_TYPE,
	NVIDIA_FIELD_ERROR_INSTANCE,
	NVIDIA_FIELD_SEVERITY,
	NVIDIA_FIELD_SOCKET,
	NVIDIA_FIELD_NUMBER_REGS,
	NVIDIA_FIELD_INSTANCE_BASE,
	NVIDIA_FIELD_REG_DATA,
};

void decode_nvidia_cper_sec(struct ras_ns_ev_decoder *ev_decoder,
			    struct trace_seq *s,
			    const struct nvidia_cper_sec *err,
			    uint32_t len);

#endif
