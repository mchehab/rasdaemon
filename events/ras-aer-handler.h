/* SPDX-License-Identifier: GPL-2.0-or-later */

/*
 * Copyright (C) 2013 Mauro Carvalho Chehab <mchehab+huawei@kernel.org>
 */

#ifndef __RAS_AER_HANDLER_H
#define __RAS_AER_HANDLER_H

#include <traceevent/event-parse.h>

#include "core/ras-events.h"

void aer_event_trigger_setup(void);
struct ras_aer_event {
	char timestamp[64];
	const char *error_type;
	uint64_t severity;
	uint64_t status;
	char *dev_name;
	uint8_t tlp_header_valid;
	uint32_t *tlp_header;
	const char *msg;
	int erst;
	uint16_t vendor_id;
	uint16_t device_id;
};

#endif
