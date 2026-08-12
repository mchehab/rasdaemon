// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2026 Mauro Carvalho Chehab <mchehab+huawei@kernel.org>
 */

#include <assert.h>
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
	const struct ras_module_entry entry = {
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

	const struct ras_module_entry mods[] = {
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

	const struct ras_module_entry mods[] = {
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

	const struct ras_module_entry mods[] = {
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

struct ras_events {
	int count;

	int errors;

	int last_level;

	const char **name;
};

int test_module_init(const char *name, struct ras_events *ras, void **priv)
{
	ras->name[ras->count] = name;
	ras->count++;

	*priv = ras;

	return 0;
}

void test_module_cleanup(const struct ras_module_entry *entry, void *priv)
{
	struct ras_events *ras = priv;
	int i;

	for (i = 0; i < ras->count; i++)
		if (!strcmp(entry->name, ras->name[i]))
			break;

	if (i >= ras->count) {
		ras->errors++;
	} else {
		if (entry->level < ras->last_level)
			ras->errors++;
		else
			ras->last_level = entry->level;
	}
}

int test_init_cleanup(void)
{
	struct ras_module_entry_runtime *entry;
	struct ras_events ras = { 0 };
	int rc = 0;
	int **idx;

	const struct ras_module_entry mods[] = {
		{
			.name   = "beta",
			.init   = test_module_init,
			.cleanup = test_module_cleanup,
			.level  = SUB_EVENT_MODULE,
		}, {
			.name   = "echo",
			.init   = test_module_init,
			.cleanup = test_module_cleanup,
			.level  = BASE_EVENT_MODULE,
		}, {
			.name   = "charlie",
			.init   = test_module_init,
			.cleanup = test_module_cleanup,
			.level  = CORE_MODULE,
		}, {
			.name   = "delta",
			.init   = test_module_init,
			.cleanup = test_module_cleanup,
			.level  = DB_MODULE,
		}, {
			.name   = "foxtrot",
			.init   = test_module_init,
			.cleanup = test_module_cleanup,
			.level  = CORE_MODULE,
		}, {
			.name   = "alpha",
			.init   = test_module_init,
			.cleanup = test_module_cleanup,
			.level  = CORE_MODULE,
		},
	};
	const char *names[] = {
		"alpha",
		"beta",
		"charlie",
		"delta",
		"echo",
		"foxtrot",
	};

	ras.name = calloc(ARRAY_SIZE(mods), sizeof(const char *));

	for (int i = 0; i < ARRAY_SIZE(mods); i++) {
		rc |= module_register(&mods[i]);
		rc |= check(rc == 0);
	}

	modules_init(&ras);

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

	rc |= check(ras.errors == 0);

	modules_unregister();
	rc |= check(ras_modules.head == NULL);

	return rc;
}

/*
 * Unit test runner
 */

static struct test_case tests[] = {
	{ test_register_null_entry,		"NULL entry" },
	{ test_register_single_module,		"single module" },
	{ test_register_modules_in_order,	"modules in order" },
	{ test_register_mixed_order,		"modules out of order" },
	{ test_register_muptiple_levels,	"multiple levels" },
	{ test_init_cleanup,			"init/cleanup" },
};

int test_modules(void)
{
	modules_unregister();
	return run_tests("modules support", tests,  ARRAY_SIZE(tests));
}
