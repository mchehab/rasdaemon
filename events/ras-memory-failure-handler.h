/* SPDX-License-Identifier: GPL-2.0-or-later */

/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2020. All rights reserved.
 */

#ifndef __RAS_MEMORY_FAILURE_HANDLER_H
#define __RAS_MEMORY_FAILURE_HANDLER_H

#include <traceevent/event-parse.h>

#include "core/ras-events.h"

struct ras_mf_event {
	char timestamp[64];
	char pfn[30];
	const char *page_type;
	const char *action_result;
};

#endif
