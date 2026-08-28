/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2026 Mauro Carvalho Chehab <mchehab+huawei@kernel.org>
 */

#ifndef __RAS_ARM_VENDOR_DATA_H
#define __RAS_ARM_VENDOR_DATA_H

#include <stdbool.h>
#include <stdint.h>
#include <traceevent/event-parse.h>

struct ras_arm_vendor_data_handler {
	const char *name;
	int64_t midr;
	void (*decode)(struct trace_seq *s, const uint8_t *buf, uint32_t length);
};

int ras_arm_vendor_data_register(const struct ras_arm_vendor_data_handler *handler);
void ras_arm_vendor_data_unregister(const struct ras_arm_vendor_data_handler *handler);
bool ras_arm_vendor_data_decode(int64_t midr, struct trace_seq *s,
				const uint8_t *buf, uint32_t length);

#endif
