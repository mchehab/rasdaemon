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

/**
 * struct ras_event_consumer_runtime - registry wrapper for a consumer
 * @consumer: static consumer descriptor
 * @node: link in event_consumers
 */
struct ras_event_consumer_runtime {
	const struct ras_event_consumer *consumer;

	LIST_ENTRY(ras_event_consumer_runtime) node;
};

LIST_HEAD(ras_event_consumer_list, ras_event_consumer_runtime);

/**
 * var event_consumers - consumers ordered by priority and name
 */
static struct ras_event_consumer_list event_consumers =
	LIST_HEAD_INITIALIZER(event_consumers);

/**
 * ras_event_consumers_unregister - free registry wrappers at process exit
 */
static void ras_event_consumers_unregister(void)
{
	struct ras_event_consumer_runtime *consumer;

	while ((consumer = LIST_FIRST(&event_consumers))) {
		LIST_REMOVE(consumer, node);
		free(consumer);
	}
}

/**
 * ras_event_consumer_register - register an immutable event consumer
 * @consumer: static descriptor
 *
 * Registration occurs during constructors and is not thread-safe. Consumers
 * are ordered by ascending priority and then name.
 *
 * Return:
 * * 0 - the consumer was registered
 * * -EINVAL - @consumer or one of its required fields is invalid
 * * -EEXIST - its descriptor or name is already registered
 * * -ENOMEM - wrapper allocation or exit-handler registration failed
 */
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

/**
 * ras_event_consumer_unregister - remove a registered consumer
 * @consumer: descriptor previously passed to ras_event_consumer_register()
 *
 * Return:
 * * 0 - the consumer was unregistered
 * * -EINVAL - @consumer is NULL
 * * -ENOENT - @consumer is not registered
 */
int ras_event_consumer_unregister(const struct ras_event_consumer *consumer)
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

/**
 * ras_event_publish - synchronously deliver a decoded event
 * @ras: event-loop context
 * @event: event identifier from enum ras_event_id
 * @data: publisher-owned event payload
 *
 * Every interested consumer runs even if an earlier consumer fails. @data is
 * valid only for the duration of this call. Callers must serialize publishing
 * if a consumer requires it.
 *
 * Return:
 * * 0 - every interested consumer succeeded
 * * -EINVAL - @ras, @data, or @event is invalid
 * * otherwise - the first consumer error in delivery order
 */
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
