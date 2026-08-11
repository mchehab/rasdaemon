// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2026 Mauro Carvalho Chehab <mchehab+huawei@kernel.org>
 */

#include <stdio.h>

#include "core/ras-logger.h"
#include "tests/unittest.h"

int run_tests(const char *name, const struct test_case tests[], size_t len)
{
	unsigned int i, failures = 0;

	printf("Testing %s (%d tests):\n", name, len);
	for (i = 0; i < len; i++) {
		printf("%02u/%02u:   Running %s\n", i + 1, len, tests[i].name);
		ras_logger_clean();
		if (tests[i].fn()) {
			ras_logger_flush();
			failures++;
		}
	}
	if (!failures)
		printf("%s: a tests passed.\n", name);
	else
		printf("%s: %u tests failed, %u succeeded.\n",
		       name, failures, len - failures);

	printf("\n");

	return 0;
}
