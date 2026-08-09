// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2026 Mauro Carvalho Chehab <mchehab+huawei@kernel.org>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/modules.h"
#include "tests/unittest.h"

extern struct module_list ras_modules;

int test_register_null_entry(void)
{
	int rc = module_register(NULL);
	return check(rc < 0);
}

int test_register_single_module(void)
{
	static const struct ras_module_entry entry = {
		.name   = "test-module",
		.init   = NULL,
		.cleanup = NULL,
		.level  = CORE_MODULE,
	};

	int rc = module_register(&entry);

	rc |= check(rc == 0);
	rc |= check(ras_modules.head != NULL);
	rc |= check(!strcmp(ras_modules.head->e->name, "test-module"));

	modules_unregister();
	rc |= check(ras_modules.head == NULL);

	return rc;
}

int test_register_modules_in_order(void)
{
	struct ras_module_entry_runtime *entry;
	int rc = 0;

	static const struct ras_module_entry mods[] = {
		{
			.name   = "alpha",
			.init   = NULL,
			.cleanup = NULL,
			.level  = CORE_MODULE,
		}, {
			.name   = "beta",
			.init   = NULL,
			.cleanup = NULL,
			.level  = CORE_MODULE,
		}, {
			.name   = "charlie",
			.init   = NULL,
			.cleanup = NULL,
			.level  = CORE_MODULE,
		}, {
			.name   = "delta",
			.init   = NULL,
			.cleanup = NULL,
			.level  = CORE_MODULE,
		}, {
			.name   = "echo",
			.init   = NULL,
			.cleanup = NULL,
			.level  = CORE_MODULE,
		}, {
			.name   = "foxtrot",
			.init   = NULL,
			.cleanup = NULL,
			.level  = CORE_MODULE,
		},
	};

	for (int i = 0; i < ARRAY_SIZE(mods); i++) {
		rc |= module_register(&mods[i]);
		rc |= check(rc == 0);
	}

	entry = ras_modules.head;
	for (int i = 0;  i < ARRAY_SIZE(mods); i++) {
		if (!entry) {
			fprintf(stderr,
				"FAIL: %s:%d: module #%d: %s is missing\n",
				__FILE__, __LINE__, i, mods[i].name);
			rc |= -1;
		} else {
			rc |= check(!strcmp(entry->e->name, mods[i].name));
		}

		if (entry)
			entry = entry->next;
	}

	modules_unregister();
	rc |= check(ras_modules.head == NULL);

	return rc;
}

int test_register_mixed_order(void)
{
	struct ras_module_entry_runtime *entry;
	int rc = 0;

	static const struct ras_module_entry mods[] = {
		{
			.name   = "beta",
			.init   = NULL,
			.cleanup = NULL,
			.level  = CORE_MODULE,
		}, {
			.name   = "echo",
			.init   = NULL,
			.cleanup = NULL,
			.level  = CORE_MODULE,
		}, {
			.name   = "charlie",
			.init   = NULL,
			.cleanup = NULL,
			.level  = CORE_MODULE,
		}, {
			.name   = "delta",
			.init   = NULL,
			.cleanup = NULL,
			.level  = CORE_MODULE,
		}, {
			.name   = "foxtrot",
			.init   = NULL,
			.cleanup = NULL,
			.level  = CORE_MODULE,
		}, {
			.name   = "alpha",
			.init   = NULL,
			.cleanup = NULL,
			.level  = CORE_MODULE,
		},
	};
	static const char *names[] = {
		"alpha",
		"beta",
		"charlie",
		"delta",
		"echo",
		"foxtrot",
	};

	for (int i = 0; i < ARRAY_SIZE(mods); i++) {
		rc |= module_register(&mods[i]);
		rc |= check(rc == 0);
	}

	entry = ras_modules.head;
	for (int i = 0;  i < ARRAY_SIZE(mods); i++) {
		if (!entry) {
			fprintf(stderr,
				"FAIL: %s:%d: module #%d: %s is missing\n",
				__FILE__, __LINE__, i, names[i]);
			rc |= -1;
		} else {
			rc |= check(!strcmp(entry->e->name, names[i]));
		}

		if (entry)
			entry = entry->next;
	}

	modules_unregister();
	rc |= check(ras_modules.head == NULL);

	return rc;
}

int test_register_muptiple_levels(void)
{
	struct ras_module_entry_runtime *entry;
	int rc = 0;

	static const struct ras_module_entry mods[] = {
		{
			.name   = "bitfield",
			.init   = NULL,
			.cleanup = NULL,
			.level  = CORE_MODULE,
		}, {
			.name   = "db-backend",
			.init   = NULL,
			.cleanup = NULL,
			.level  = DB_MODULE,
		},
	};

	for (int i = 0; i < ARRAY_SIZE(mods); i++) {
		rc |= module_register(&mods[i]);
		rc |= check(rc == 0);
	}

	entry = ras_modules.head;
	for (int i = 0;  i < ARRAY_SIZE(mods); i++) {
		if (!entry) {
			fprintf(stderr,
				"FAIL: %s:%d: module %s is missing\n",
				__FILE__, __LINE__, mods[i].name);
			rc |= -1;
		} else {
			rc |= check(!strcmp(entry->e->name, mods[i].name));
			rc |= check(entry->e->level == mods[i].level);
		}

		if (entry)
			entry = entry->next;
	}

	modules_unregister();
	rc |= check(ras_modules.head == NULL);

	return rc;
}

struct test_case tests[] = {
	{ test_register_null_entry,		"NULL entry" },
	{ test_register_single_module,		"single module" },
	{ test_register_modules_in_order,	"modules in order" },
	{ test_register_mixed_order,		"modules out of order" },
	{ test_register_muptiple_levels,	"multiple levels" },
};

/*
 * Unit test runner
 */

int test_modules(void)
{
	unsigned int n_tests = ARRAY_SIZE(tests);
	unsigned int i, failures = 0;

	printf("Testing modules functionality:\n");


	for (i = 0; i < n_tests; i++) {
		printf("%02i/%02i:   Running %s\n",
		       i + 1, n_tests, tests[i].name);

		if (tests[i].fn())
			failures++;
	}
	if (!failures)
		printf("All module registration tests passed.\n");
	else
		printf("%i tests failed, %i succeded.\n",
		       failures, n_tests - failures);

	return 0;
}
