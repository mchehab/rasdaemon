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

static void assert_module(struct ras_module_entry_runtime *module,
			  int pos, const char *name)
{
	char msg[256];

	snprintf(msg, sizeof(msg),
		 "FAIL: %s:%d: module #%d: %s is missing",
		 __FILE__, __LINE__, pos, name);

	assert_non_null_msg(module, msg);

	if (module)
		assert_string_equal(module->e->name, name);
}

static void test_register_null_entry(void **state)
{
	int rc = module_register(NULL);
	assert_int_not_equal(rc, 0);
}

static void test_register_single_module(void **state)
{
	const struct ras_module_entry entry = {
		.name   = "test-module",
		.init   = NULL,
		.cleanup = NULL,
		.level  = CORE_MODULE,
	};

	int rc = module_register(&entry);

	assert_int_equal(rc, 0);
	assert_non_null(ras_modules.head);
	assert_string_equal(ras_modules.head->e->name, "test-module");

	modules_unregister();
	assert_null(ras_modules.head);
}

static void test_register_modules_in_order(void **state)
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
		rc = module_register(&mods[i]);
		assert_int_equal(rc, 0);
	}

	entry = ras_modules.head;
	for (int i = 0;  i < ARRAY_SIZE(mods); i++) {
		assert_module(entry, i, mods[i].name);
		if (entry)
			entry = entry->next;
	}

	modules_unregister();
	assert_null(ras_modules.head);
}

static void test_register_mixed_order(void **state)
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
		rc = module_register(&mods[i]);
		assert_int_equal(rc, 0);
	}

	entry = ras_modules.head;
	for (int i = 0;  i < ARRAY_SIZE(mods); i++) {
		assert_module(entry, i, names[i]);
		if (entry)
			entry = entry->next;
	}

	modules_unregister();
	assert_null(ras_modules.head);
}

static void test_register_muptiple_levels(void **state)
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
		module_register(&mods[i]);
		assert_int_equal(rc, 0);
	}

	entry = ras_modules.head;
	for (int i = 0;  i < ARRAY_SIZE(mods); i++) {
		assert_module(entry, i, mods[i].name);

		if (entry) {
			assert_int_equal(entry->e->level, mods[i].level);
			entry = entry->next;
		}
	}

	modules_unregister();
	assert_null(ras_modules.head);
}

struct ras_events {
	int count;

	int errors;

	int cleanup_count;

	int cleanup_level[MAX_LEVELS + 2];

	const char **name;
};

static int test_module_init(const char *name, struct ras_events *ras, void **priv)
{
	ras->name[ras->count] = name;
	ras->count++;

	*priv = ras;

	return 0;
}

static void test_module_cleanup(const struct ras_module_entry *entry, void *priv)
{
	struct ras_events *ras = priv;
	int i;

	for (i = 0; i < ras->count; i++)
		if (!strcmp(entry->name, ras->name[i]))
			break;

	if (i >= ras->count) {
		ras->errors++;
	} else {
		ras->cleanup_level[ras->cleanup_count++] = entry->level;
	}
}

static int failing_module_init(const char *name, struct ras_events *ras,
			       void **priv)
{
	(void)name;
	(void)ras;
	(void)priv;
	return -EINVAL;
}

static void test_named_module_lifecycle(void **state)
{
	struct ras_events ras = { 0 };
	const char *names[1] = { 0 };
	const struct ras_module_entry entry = {
		.name = "named",
		.init = test_module_init,
		.cleanup = test_module_cleanup,
		.level = CORE_MODULE,
	};

	(void)state;
	ras.name = names;
	assert_int_equal(module_register(&entry), 0);
	assert_false(module_is_enabled("named"));
	assert_true(module_init(&ras, "missing"));
	assert_false(module_init(&ras, "named"));
	assert_true(module_is_enabled("named"));
	assert_false(module_cleanup("named"));
	assert_false(module_is_enabled("named"));
	assert_int_equal(ras.cleanup_count, 1);
	assert_true(module_cleanup("named"));
	modules_unregister();
}

static void test_failed_and_postponed_init(void **state)
{
	struct ras_events ras = { 0 };
	const struct ras_module_entry failed = {
		.name = "failed",
		.init = failing_module_init,
		.level = CORE_MODULE,
	};
	const struct ras_module_entry postponed = {
		.name = "postponed",
		.init = failing_module_init,
		.level = CORE_MODULE,
		.postpone_init = true,
	};

	(void)state;
	assert_int_equal(module_register(&failed), 0);
	assert_int_equal(module_register(&postponed), 0);
	assert_true(module_init(&ras, "failed"));
	assert_false(module_is_enabled("failed"));
	assert_true(module_init(&ras, "postponed"));
	modules_init(&ras);
	assert_false(module_is_enabled("failed"));
	assert_false(module_is_enabled("postponed"));
	modules_unregister();
}

static void test_sql_backend_state(void **state)
{
	struct ras_events ras = { 0 };
	const char *names[1] = { 0 };
	const struct ras_module_entry entry = {
		.name = "test-db",
		.init = test_module_init,
		.cleanup = test_module_cleanup,
		.level = DB_MODULE,
	};

	(void)state;
	ras.name = names;
	assert_int_equal(module_register(&entry), 0);
	assert_false(modules_have_sql_backend());
	assert_false(module_init(&ras, "test-db"));
	assert_true(modules_have_sql_backend());
	assert_false(module_cleanup("test-db"));
	assert_false(modules_have_sql_backend());
	modules_unregister();
}

static void test_init_cleanup(void **state)
{
	struct ras_module_entry_runtime *entry;
	struct ras_events ras = { 0 };
	int rc = 0;

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
		rc = module_register(&mods[i]);
		assert_int_equal(rc, 0);
	}

	modules_init(&ras);

	entry = ras_modules.head;
	for (int i = 0;  i < ARRAY_SIZE(mods); i++) {
		assert_module(entry, i, names[i]);

		if (entry)
			entry = entry->next;
	}

	assert_int_equal(ras.errors, 0);

	modules_unregister();
	assert_int_equal(ras.cleanup_count, ARRAY_SIZE(mods));
	for (int i = 1; i < ras.cleanup_count; i++)
		assert_true(ras.cleanup_level[i - 1] >= ras.cleanup_level[i]);
	assert_null(ras_modules.head);
	assert_null(ras_modules.next);
	free(ras.name);
	ras.name = NULL;
}

/*
 * Unit test runner
 */

static const struct CMUnitTest tests[] = {
	cmocka_unit_test(test_register_null_entry),
	cmocka_unit_test(test_register_single_module),
	cmocka_unit_test(test_register_modules_in_order),
	cmocka_unit_test(test_register_mixed_order),
	cmocka_unit_test(test_register_muptiple_levels),
	cmocka_unit_test(test_named_module_lifecycle),
	cmocka_unit_test(test_failed_and_postponed_init),
	cmocka_unit_test(test_sql_backend_state),
	cmocka_unit_test(test_init_cleanup),
};

int test_modules(void)
{
	modules_unregister();

	return _cmocka_run_group_tests("modules support",
				       tests,
				       ARRAY_SIZE(tests),
				       NULL,
				       NULL);
}
