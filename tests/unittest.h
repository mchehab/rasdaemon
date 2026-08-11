// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2026 Mauro Carvalho Chehab <mchehab+huawei@kernel.org>
 */

#include "core/types.h"

#define check(cond)							\
	({								\
		int __rc;						\
		do {							\
			if (!(cond)) {					\
				fprintf(stderr, "\tFAIL: %s:%d: %s\n",	\
					__FILE__, __LINE__, #cond);	\
				__rc = -1;				\
			} else {					\
				__rc = 0;				\
			}						\
		} while (0);						\
		__rc;							\
	})


struct test_case {
	int (*fn)(void);
	const char *name;
};

int run_tests(const char *name, const struct test_case tests[], size_t len);
