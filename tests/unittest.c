// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2026 Mauro Carvalho Chehab <mchehab+huawei@kernel.org>
 */

#include <stdio.h>

#include "core/ras-logger.h"
#include "tests/unittest.h"

#include "core/modules.h"

int run_tests(const char *name, const struct test_case tests[], size_t len)
{
	unsigned int i, success = 0, failures;

	printf("Testing %s (%d tests):\n", name, len);
	for (i = 0; i < len; i++) {
		printf("%02u/%02u:   Running %s\n", i + 1, len, tests[i].name);
		ras_logger_clean();
		if (tests[i].fn()) {
			ras_logger_flush();
			if (tests[i].fatal) {
				printf("\tFATAL error. Aborting other tests.\n");
				break;
			}
		} else {
			success++;
		}
	}
	failures = len - success;
	if (!failures)
		printf("%s: all tests passed.\n", name);
	else
		printf("%s: %u tests failed, %u succeeded.\n",
		       name, failures, success);

	printf("\n");

	return 0;
}
