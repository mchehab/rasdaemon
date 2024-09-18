/* SPDX-License-Identifier: GPL-2.0-or-later */

/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2021. All rights reserved.
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
