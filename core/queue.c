// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2021. All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>

#include "core/queue.h"
#include "core/ras-logger.h"

/**
 * is_empty - test whether a queue has no entries
 * @queue: queue to inspect, or NULL
 *
 * Return:
 * nonzero when @queue is NULL or empty.
 */
int is_empty(struct link_queue *queue)
{
	if (queue)
		return queue->size == 0;

	return 1;
}

/**
 * init_queue - allocate an empty queue
 *
 * Return:
 * a queue owned by the caller, or NULL on allocation failure.
 */
struct link_queue *init_queue(void)
{
	struct link_queue *queue = NULL;

	queue = (struct link_queue *)malloc(sizeof(struct link_queue));
	if (!queue) {
		log(TERM, LOG_ERR, "Failed to allocate memory for queue.\n");
		return NULL;
	}

	queue->size = 0;
	queue->head = NULL;
	queue->tail = NULL;

	return queue;
}

/**
 * clear_queue - free every node in a queue
 * @queue: queue to empty, or NULL
 *
 * The queue object remains valid and can be reused.
 */
void clear_queue(struct link_queue *queue)
{
	if (!queue)
		return;

	struct queue_node *node = queue->head;
	struct queue_node *tmp = NULL;

	while (node) {
		tmp = node;
		node = node->next;
		free(tmp);
	}

	queue->head = NULL;
	queue->tail = NULL;
	queue->size = 0;
}

/**
 * free_queue - destroy a queue and all its nodes
 * @queue: queue to destroy, or NULL
 */
void free_queue(struct link_queue *queue)
{
	clear_queue(queue);

	if (queue)
		free(queue);
}

/**
 * push - append a node to a queue
 * @queue: valid queue
 * @node: detached node whose ownership transfers to @queue
 *
 * Both arguments must be non-NULL.
 */
void push(struct link_queue *queue, struct queue_node *node)
{
	/* there is no element in the queue */
	if (!queue->head)
		queue->head = node;
	else
		queue->tail->next = node;

	queue->tail = node;
	(queue->size)++;
}

/**
 * pop - remove and free the first queue node
 * @queue: queue to modify
 *
 * Return:
 * * 0 - the first node was removed and freed
 * * -1 - @queue is NULL or empty
 */
int pop(struct link_queue *queue)
{
	struct queue_node *tmp = NULL;

	if (!queue || is_empty(queue))
		return -1;

	tmp = queue->head;
	queue->head = queue->head->next;
	free(tmp);
	(queue->size)--;
	if (!queue->head)
		queue->tail = NULL;

	return 0;
}

/**
 * front - obtain the first queue node without removing it
 * @queue: queue to inspect
 *
 * Return:
 * a queue-owned node, or NULL if @queue is NULL or empty.
 */
struct queue_node *front(struct link_queue *queue)
{
	if (!queue)
		return NULL;

	return queue->head;
}

/**
 * node_create - allocate a detached queue node
 * @time: timestamp stored in the node
 * @value: numeric value stored in the node
 *
 * Return:
 * a caller-owned node, or NULL on allocation failure.
 */
struct queue_node *node_create(time_t time, unsigned int value)
{
	struct queue_node *node = NULL;

	node = (struct queue_node *)malloc(sizeof(struct queue_node));
	if (node) {
		node->time = time;
		node->value = value;
		node->next = NULL;
	}

	return node;
}
