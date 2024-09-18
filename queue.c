// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2021. All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>

#include "queue.h"
#include "ras-logger.h"

int is_empty(struct link_queue *queue)
{
	if (queue)
		return queue->size == 0;

	return 1;
}

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

void free_queue(struct link_queue *queue)
{
	clear_queue(queue);

	if (queue)
		free(queue);
}

/* It should be guaranteed that the param is not NULL */
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

int pop(struct link_queue *queue)
{
	struct queue_node *tmp = NULL;

	if (!queue || is_empty(queue))
		return -1;

	tmp = queue->head;
	queue->head = queue->head->next;
	free(tmp);
	(queue->size)--;

	return 0;
}

struct queue_node *front(struct link_queue *queue)
{
	if (!queue)
		return NULL;

	return queue->head;
}

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
