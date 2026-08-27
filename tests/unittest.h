// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2026 Mauro Carvalho Chehab <mchehab+huawei@kernel.org>
 */

#include <setjmp.h>
#include <stddef.h>
#include <stdint.h>

#include <cmocka.h>
#include <traceevent/event-parse.h>

#include "core/modules.h"
#include "core/types.h"

struct ras_events;

#ifndef CMOCKA_VERSION_2
#define assert_non_null_msg(x, y) assert_non_null(x)
#define assert_null_msg(x, y) assert_null(x)
#endif

#ifndef TEST_GROUPS_H
#define TEST_GROUPS_H

#define REGISTER_TEST(group, function, priority) \
	static void __attribute__((constructor)) register_##function(void) \
	{ \
		module_test_register(group, function, priority); \
	}

void test_database_tables(void **state);
void test_ras_mc_ctl_count(const char *backend, const char *table,
			   int expected);
void test_ras_mc_ctl_types(const char *backend, struct ras_events *ras);
tep_event_handler_func ras_event_test_handler(const char *group,
					      const char *event);
int ras_event_test_record(const char *group, const char *event,
			  struct ras_events *ras, void *data);

#endif
