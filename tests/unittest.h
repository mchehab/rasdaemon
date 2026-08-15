// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2026 Mauro Carvalho Chehab <mchehab+huawei@kernel.org>
 */

#include <cmocka.h>

#include <core/types.h>

#ifndef TEST_GROUPS_H
#define TEST_GROUPS_H

struct test_group {
	const char *name;
	int (*run)(void);
};

int test_modules(void);
int test_mysql(void);
int test_sqlite3(void);

#endif
