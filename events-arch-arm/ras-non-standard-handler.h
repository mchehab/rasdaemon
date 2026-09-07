/* SPDX-License-Identifier: GPL-2.0-only */

/*
 * Copyright (c) 2016, The Linux Foundation. All rights reserved.
 */

#ifndef __RAS_NON_STANDARD_HANDLER_H
#define __RAS_NON_STANDARD_HANDLER_H

#include <traceevent/event-parse.h>

#include "core/ras-events.h"

struct ras_non_standard_event;

struct ras_ns_ev_decoder {
	struct ras_ns_ev_decoder *next;
	const char *sec_type;
	int (*decode)(struct ras_events *ras, struct ras_ns_ev_decoder *ev_decoder,
		      struct trace_seq *s, struct ras_non_standard_event *event);
};

int register_ns_ev_decoder(struct ras_ns_ev_decoder *ns_ev_decoder);
void unregister_ns_ev_decoder(struct ras_ns_ev_decoder *ns_ev_decoder);

struct ras_non_standard_event {
	char timestamp[64];
	const char *sec_type, *fru_id, *fru_text;
	const char *severity;
	const uint8_t *error;
	uint32_t length;
};

#ifdef HAVE_UNITTEST
size_t ras_ns_test_decoder_count(void);
const char *ras_ns_test_decoder_type(size_t index);
int ras_ns_test_decode(const char *type, struct ras_events *ras,
		       struct trace_seq *seq,
		       struct ras_non_standard_event *event);
#endif

#endif
