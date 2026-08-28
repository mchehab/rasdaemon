/* SPDX-License-Identifier: GPL-2.0-or-later */

/*
 * Copyright (C) 2013 Mauro Carvalho Chehab <mchehab+huawei@kernel.org>
 */

#ifndef __RAS_MC_HANDLER_H
#define __RAS_MC_HANDLER_H

#include <traceevent/event-parse.h>

#include "core/ras-events.h"

extern unsigned long long mc_ce_stat_threshold;

void mc_event_trigger_setup(void);

struct ras_mc_event {
	char timestamp[64];
	int error_count;
	const char *error_type, *msg, *label;
	unsigned char mc_index;
	signed char top_layer, middle_layer, lower_layer;
	unsigned long long address, grain, syndrome;
	const char *driver_detail;
	int erst;
};

#endif
