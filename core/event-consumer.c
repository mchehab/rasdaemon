// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2026 Mauro Carvalho Chehab <mchehab+huawei@kernel.org>
 */

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/queue.h>

#include "core/ras-events.h"

struct ras_event_consumer_runtime {
	const struct ras_event_consumer *consumer;

	LIST_ENTRY(ras_event_consumer_runtime) node;
};

LIST_HEAD(ras_event_consumer_list, ras_event_consumer_runtime);

static struct ras_event_consumer_list event_consumers =
	LIST_HEAD_INITIALIZER(event_consumers);

static void ras_event_consumers_unregister(void)
{
	struct ras_event_consumer_runtime *consumer;

	while ((consumer = LIST_FIRST(&event_consumers))) {
		LIST_REMOVE(consumer, node);
		free(consumer);
	}
}

int ras_event_consumer_register(const struct ras_event_consumer *consumer)
{
	struct ras_event_consumer_runtime *entry, *new, *prev = NULL;
	static int cleanup_registered;
	int cmp;

	if (!consumer || !consumer->name || !consumer->events ||
	    !consumer->consume || consumer->priority < PRI_CPU_ISOLATION ||
	    consumer->priority > PRI_NORMAL)
		return -EINVAL;

	LIST_FOREACH(entry, &event_consumers, node) {
		if (entry->consumer == consumer ||
		    !strcmp(entry->consumer->name, consumer->name))
			return -EEXIST;

		if (consumer->priority < entry->consumer->priority)
			cmp = -1;
		else if (consumer->priority > entry->consumer->priority)
			cmp = 1;
		else
			cmp = strcmp(consumer->name, entry->consumer->name);

		if (cmp < 0)
			break;
		prev = entry;
	}

	new = calloc(1, sizeof(*new));
	if (!new)
		return -ENOMEM;
	new->consumer = consumer;
	if (prev)
		LIST_INSERT_AFTER(prev, new, node);
	else
		LIST_INSERT_HEAD(&event_consumers, new, node);

	if (!cleanup_registered) {
		if (atexit(ras_event_consumers_unregister)) {
			LIST_REMOVE(new, node);
			free(new);
			return -ENOMEM;
		}
		cleanup_registered = 1;
	}

	return 0;
}

#ifdef HAVE_UNITTEST
int ras_event_consumer_test_unregister(
		const struct ras_event_consumer *consumer)
{
	struct ras_event_consumer_runtime *entry;

	if (!consumer)
		return -EINVAL;

	LIST_FOREACH(entry, &event_consumers, node) {
		if (entry->consumer != consumer)
			continue;

		LIST_REMOVE(entry, node);
		free(entry);
		return 0;
	}

	return -ENOENT;
}
#endif

int ras_event_publish(struct ras_events *ras, int event, void *data)
{
	struct ras_event_consumer_runtime *entry;
	uint64_t event_mask;
	int first_error = 0;
	int rc;

	if (!ras || !data || event < 0 || event >= NR_EVENTS)
		return -EINVAL;

	event_mask = BIT_ULL(event);
	LIST_FOREACH(entry, &event_consumers, node) {
		if (!(entry->consumer->events & event_mask))
			continue;

		rc = entry->consumer->consume(ras, event, data);
		if (rc && !first_error)
			first_error = rc;
	}

	return first_error;
}
