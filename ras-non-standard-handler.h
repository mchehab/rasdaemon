/* SPDX-License-Identifier: GPL-2.0 */

/*
 * Copyright (c) 2016, The Linux Foundation. All rights reserved.
 */

#ifndef __RAS_NON_STANDARD_HANDLER_H
#define __RAS_NON_STANDARD_HANDLER_H

#include <traceevent/event-parse.h>

#include "ras-events.h"
#include "ras-record.h"

#ifdef HAVE_SQLITE3
#include <sqlite3.h>
#endif

struct ras_ns_ev_decoder {
	struct ras_ns_ev_decoder *next;
	uint16_t ref_count;
	const char *sec_type;
	int (*add_table)(struct ras_events *ras, struct ras_ns_ev_decoder *ev_decoder);
	int (*decode)(struct ras_events *ras, struct ras_ns_ev_decoder *ev_decoder,
		      struct trace_seq *s, struct ras_non_standard_event *event);
#ifdef HAVE_SQLITE3
	struct sqlite3_stmt *stmt_dec_record;
#endif
};

int ras_non_standard_event_handler(struct trace_seq *s,
				   struct tep_record *record,
				   struct tep_event *event, void *context);

void print_le_hex(struct trace_seq *s, const uint8_t *buf, int index);

#ifdef HAVE_NON_STANDARD
int register_ns_ev_decoder(struct ras_ns_ev_decoder *ns_ev_decoder);
int ras_ns_add_vendor_tables(struct ras_events *ras);
void ras_ns_finalize_vendor_tables(void);
#else
static inline int register_ns_ev_decoder(struct ras_ns_ev_decoder *ns_ev_decoder) { return 0; };
#endif

#endif
