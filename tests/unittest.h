// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2026 Mauro Carvalho Chehab <mchehab+huawei@kernel.org>
 */

#include <setjmp.h>

#include <cmocka.h>

#include <core/types.h>

#ifndef CMOCKA_VERSION_2
#define assert_non_null_msg(x, y) assert_non_null(x)
#define assert_null_msg(x, y) assert_null(x)
#endif

#ifndef TEST_GROUPS_H
#define TEST_GROUPS_H

struct test_group {
	const char *name;
	int (*run)(void);
};

int test_modules(void);
int test_core(void);
int test_mc(void);
int test_report(void);
int test_aer(void);
int test_amp_ns(void);
int test_arm(void);
int test_cpu_isolation(void);
int test_cxl(void);
int test_devlink(void);
int test_diskerror(void);
int test_erst(void);
int test_extlog(void);
int test_hisi_ns(void);
int test_jaguar_ns(void);
int test_mce(void);
int test_memory_ce_pfa(void);
int test_memory_failure(void);
int test_memory_row_ce_pfa(void);
int test_nvidia_ns(void);
int test_openbmc_sel(void);
int test_reri(void);
int test_signal(void);
int test_yitian_ns(void);
int test_database(void);
int test_mysql(void);
int test_postgresql(void);
int test_sqlite3(void);
void test_database_tables(void **state);

#endif
