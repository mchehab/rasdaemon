/*
 * Copyright (c) 2016, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __RAS_NON_STANDARD_HANDLER_H
#define __RAS_NON_STANDARD_HANDLER_H

#include "ras-events.h"
#include <traceevent/event-parse.h>

struct ras_ns_ev_decoder {
	struct ras_ns_ev_decoder *next;
	const char *sec_type;
	int (*decode)(struct ras_events *ras, struct ras_ns_ev_decoder *ev_decoder,
		      struct trace_seq *s, struct ras_non_standard_event *event);
#ifdef HAVE_SQLITE3
#include <sqlite3.h>
	sqlite3_stmt *stmt_dec_record;
#endif
};

int ras_non_standard_event_handler(struct trace_seq *s,
				   struct tep_record *record,
				   struct tep_event *event, void *context);

void print_le_hex(struct trace_seq *s, const uint8_t *buf, int index);

#ifdef HAVE_NON_STANDARD
int register_ns_ev_decoder(struct ras_ns_ev_decoder *ns_ev_decoder);
#else
static inline int register_ns_ev_decoder(struct ras_ns_ev_decoder *ns_ev_decoder) { return 0; };
#endif

#endif
