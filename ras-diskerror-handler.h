/* SPDX-License-Identifier: GPL-2.0-or-later */

/*
 * Copyright (C) 2019 Cong Wang <xiyou.wangcong@gmail.com>
 */

#ifndef __RAS_DISKERROR_HANDLER_H
#define __RAS_DISKERROR_HANDLER_H

#include <traceevent/event-parse.h>

#include "ras-events.h"

/*
 * Defined in linux/kdev_t.h
 * Since the encoding with 20 bit mask is not defined in the linux-libc headers,
 * redefine it here
 * format:
 *	field:unsigned short common_type;	offset:0;	size:2;	signed:0;
 *	field:unsigned char common_flags;	offset:2;	size:1;	signed:0;
 *	field:unsigned char common_preempt_count;	offset:3;	size:1;	signed:0;
 *	field:int common_pid;	offset:4;	size:4;	signed:1;
 *
 *	field:dev_t dev;	offset:8;	size:4;	signed:0;
 *	field:sector_t sector;	offset:16;	size:8;	signed:0;
 *	field:unsigned int nr_sector;	offset:24;	size:4;	signed:0;
 *	field:int error;	offset:28;	size:4;	signed:1;
 *	field:unsigned short ioprio;	offset:32;	size:2;	signed:0;
 *	field:char rwbs[8];	offset:34;	size:8;	signed:0;
 *	field:__data_loc char[] cmd;	offset:44;	size:4;	signed:0;
 *
 * print fmt: "%d,%d %s (%s) %llu + %u %s,%u,%u [%d]", ((unsigned int) ((REC->dev) >> 20)), ((unsigned int) ((REC->dev) & ((1U << 20) - 1))), REC->rwbs, __get_str(cmd), (unsigned long long)REC->sector, REC->nr_sector, __print_symbolic((((REC->ioprio) >> 13) & (8 - 1)), { IOPRIO_CLASS_NONE, "none" }, { IOPRIO_CLASS_RT, "rt" }, { IOPRIO_CLASS_BE, "be" }, { IOPRIO_CLASS_IDLE, "idle" }, { IOPRIO_CLASS_INVALID, "invalid"}), (((REC->ioprio) >> 3) & ((1 << 10) - 1)), ((REC->ioprio) & ((1 << 3) - 1)), REC->error
 */
#define MINORBITS	20
#define MINORMASK	((1U << MINORBITS) - 1)

#define MAJOR(dev)	((unsigned int) ((dev) >> MINORBITS))
#define MINOR(dev)	((unsigned int) ((dev) & MINORMASK))
#define MKDEV(ma,mi)	(((ma) << MINORBITS) | (mi))
int ras_diskerror_event_handler(struct trace_seq *s,
				struct tep_record *record,
				struct tep_event *event, void *context);

#endif
