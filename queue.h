/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2021. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __RAS_QUEUE_H
#define __RAS_QUEUE_H

struct queue_node {
	time_t time;
	unsigned int value;
	struct queue_node *next;
};

struct link_queue {
	struct queue_node *head;
	struct queue_node *tail;
	int size;
};

int is_empty(struct link_queue *queue);
struct link_queue *init_queue(void);
void clear_queue(struct link_queue *queue);
void free_queue(struct link_queue *queue);
void push(struct link_queue *queue, struct queue_node *node);
int pop(struct link_queue *queue);
struct queue_node *front(struct link_queue *queue);
struct queue_node *node_create(time_t time, unsigned int value);

#endif
