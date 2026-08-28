// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2026 Mauro Carvalho Chehab <mchehab+huawei@kernel.org>
 */

#include <errno.h>
#include <stdlib.h>
#include <sys/queue.h>

#include "events-arch-arm/ras-arm-vendor-data.h"

struct ras_arm_vendor_data_entry {
	const struct ras_arm_vendor_data_handler *handler;

	LIST_ENTRY(ras_arm_vendor_data_entry) node;
};

static LIST_HEAD(, ras_arm_vendor_data_entry) ras_arm_vendor_data_handlers =
	LIST_HEAD_INITIALIZER(ras_arm_vendor_data_handlers);

int ras_arm_vendor_data_register(const struct ras_arm_vendor_data_handler *handler)
{
	struct ras_arm_vendor_data_entry *entry, *new;

	if (!handler || !handler->name || !handler->decode || handler->midr < -1)
		return -EINVAL;

	LIST_FOREACH(entry, &ras_arm_vendor_data_handlers, node) {
		if (entry->handler == handler ||
		    entry->handler->midr == handler->midr)
			return -EEXIST;
	}

	new = calloc(1, sizeof(*new));
	if (!new)
		return -ENOMEM;

	new->handler = handler;
	LIST_INSERT_HEAD(&ras_arm_vendor_data_handlers, new, node);
	return 0;
}

void ras_arm_vendor_data_unregister(const struct ras_arm_vendor_data_handler *handler)
{
	struct ras_arm_vendor_data_entry *entry;

	LIST_FOREACH(entry, &ras_arm_vendor_data_handlers, node) {
		if (entry->handler != handler)
			continue;

		LIST_REMOVE(entry, node);
		free(entry);
		return;
	}
}

bool ras_arm_vendor_data_decode(int64_t midr, struct trace_seq *s,
				const uint8_t *buf, uint32_t length)
{
	const struct ras_arm_vendor_data_handler *fallback = NULL;
	struct ras_arm_vendor_data_entry *entry;

	LIST_FOREACH(entry, &ras_arm_vendor_data_handlers, node) {
		if (entry->handler->midr == midr) {
			entry->handler->decode(s, buf, length);
			return true;
		}
		if (entry->handler->midr == -1)
			fallback = entry->handler;
	}

	if (!fallback)
		return false;

	fallback->decode(s, buf, length);
	return true;
}
