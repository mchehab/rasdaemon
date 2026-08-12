// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2026 Mauro Carvalho Chehab <mchehab+huawei@kernel.org>
 */

#include "core/ras-logger.h"

/*
 * Instead of creating a header file for each test, let's just add them
 * all here, in alphabetic order.
 */
int test_modules(void);
int test_sqlite3(void);

int main(void)
{
	mock_output = true;

	int rc = 0;
	rc |= test_sqlite3();

	/* Should be the last one, as it will mock with probed modules */
	rc |= test_modules();

	return rc;
}
