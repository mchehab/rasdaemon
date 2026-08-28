// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2026 Mauro Carvalho Chehab <mchehab+huawei@kernel.org>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/modules.h"
#include "tests/unittest.h"

int test_modules(void);

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
		.level  = ACTIONS_MODULE,
	};

	int rc = module_register(&entry);

	assert_int_equal(rc, 0);
	assert_true(module_is_registered("test-module"));

	modules_unregister();
	assert_false(module_is_registered("test-module"));
}

static void test_register_duplicate_module(void **state)
{
	const struct ras_module_entry first = {
		.name = "duplicate",
		.level = ACTIONS_MODULE,
	};
	const struct ras_module_entry second = {
		.name = "duplicate",
		.level = DB_MODULE,
	};

	assert_int_equal(module_register(&first), 0);
	assert_int_equal(module_register(&second), -EEXIST);
	assert_true(module_is_registered("duplicate"));
	modules_unregister();
}

static void test_register_modules_in_order(void **state)
{
	int rc = 0;

	const struct ras_module_entry mods[] = {
		{
			.name   = "alpha",
			.init   = NULL,
			.cleanup = NULL,
			.level  = ACTIONS_MODULE,
		}, {
			.name   = "beta",
			.init   = NULL,
			.cleanup = NULL,
			.level  = ACTIONS_MODULE,
		}, {
			.name   = "charlie",
			.init   = NULL,
			.cleanup = NULL,
			.level  = ACTIONS_MODULE,
		}, {
			.name   = "delta",
			.init   = NULL,
			.cleanup = NULL,
			.level  = ACTIONS_MODULE,
		}, {
			.name   = "echo",
			.init   = NULL,
			.cleanup = NULL,
			.level  = ACTIONS_MODULE,
		}, {
			.name   = "foxtrot",
			.init   = NULL,
			.cleanup = NULL,
			.level  = ACTIONS_MODULE,
		},
	};

	for (int i = 0; i < ARRAY_SIZE(mods); i++) {
		rc = module_register(&mods[i]);
		assert_int_equal(rc, 0);
	}

	for (int i = 0;  i < ARRAY_SIZE(mods); i++) {
		assert_true(module_is_registered(mods[i].name));
	}

	modules_unregister();
	for (int i = 0; i < ARRAY_SIZE(mods); i++)
		assert_false(module_is_registered(mods[i].name));
}

static void test_register_mixed_order(void **state)
{
	int rc = 0;

	const struct ras_module_entry mods[] = {
		{
			.name   = "beta",
			.init   = NULL,
			.cleanup = NULL,
			.level  = ACTIONS_MODULE,
		}, {
			.name   = "echo",
			.init   = NULL,
			.cleanup = NULL,
			.level  = ACTIONS_MODULE,
		}, {
			.name   = "charlie",
			.init   = NULL,
			.cleanup = NULL,
			.level  = ACTIONS_MODULE,
		}, {
			.name   = "delta",
			.init   = NULL,
			.cleanup = NULL,
			.level  = ACTIONS_MODULE,
		}, {
			.name   = "foxtrot",
			.init   = NULL,
			.cleanup = NULL,
			.level  = ACTIONS_MODULE,
		}, {
			.name   = "alpha",
			.init   = NULL,
			.cleanup = NULL,
			.level  = ACTIONS_MODULE,
		},
	};
	for (int i = 0; i < ARRAY_SIZE(mods); i++) {
		rc = module_register(&mods[i]);
		assert_int_equal(rc, 0);
	}

	for (int i = 0; i < ARRAY_SIZE(mods); i++)
		assert_true(module_is_registered(mods[i].name));

	modules_unregister();
	for (int i = 0; i < ARRAY_SIZE(mods); i++)
		assert_false(module_is_registered(mods[i].name));
}

static void test_register_muptiple_levels(void **state)
{
	int rc = 0;

	const struct ras_module_entry mods[] = {
		{
			.name   = "bitfield",
			.init   = NULL,
			.cleanup = NULL,
			.level  = ACTIONS_MODULE,
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

	for (int i = 0;  i < ARRAY_SIZE(mods); i++) {
		assert_true(module_is_registered(mods[i].name));
	}

	modules_unregister();
	for (int i = 0; i < ARRAY_SIZE(mods); i++)
		assert_false(module_is_registered(mods[i].name));
}

struct ras_events {
	int count;

	int errors;

	int cleanup_count;

	int cleanup_level[MAX_LEVELS + 2];

	const char **name;
	const char **cleanup_name;
};

static int test_module_init(struct ras_module_ctx *ctx)
{
	struct ras_events *ras = ctx->ras;
	const char *name = ctx->entry->name;

	ras->name[ras->count] = name;
	ras->count++;

	ctx->priv = ras;

	return 0;
}

static void test_module_cleanup(struct ras_module_ctx *ctx)
{
	struct ras_events *ras = ctx->priv;
	const char *name = ctx->entry->name;
	int i;

	for (i = 0; i < ras->count; i++)
		if (!strcmp(name, ras->name[i]))
			break;

	if (i >= ras->count) {
		ras->errors++;
	} else {
		if (ras->cleanup_name)
			ras->cleanup_name[ras->cleanup_count] = name;
		ras->cleanup_level[ras->cleanup_count++] = ctx->entry->level;
	}
}

static int failing_module_init(struct ras_module_ctx *ctx)
{
	ctx->priv = ctx->ras;
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
		.level = ACTIONS_MODULE,
	};

	ras.name = names;
	assert_int_equal(module_register(&entry), 0);
	assert_false(module_is_enabled("named"));
	assert_int_equal(module_init(&ras, NULL), -EINVAL);
	assert_int_equal(module_init(&ras, "missing"), -ENOENT);
	assert_int_equal(module_init(&ras, "named"), 0);
	assert_true(module_is_enabled("named"));
	assert_int_equal(module_cleanup("named"), 0);
	assert_false(module_is_enabled("named"));
	assert_int_equal(ras.cleanup_count, 1);
	assert_int_equal(module_cleanup(NULL), -EINVAL);
	assert_int_equal(module_cleanup("named"), -ENOENT);
	modules_unregister();
}

static void test_failed_init_isolation(void **state)
{
	struct ras_events ras = { 0 };
	struct ras_module_ctx *ctx;
	const struct ras_module_entry failed = {
		.name = "failed",
		.init = failing_module_init,
		.level = ACTIONS_MODULE,
	};
	const char *names[3] = { 0 };
	const char *cleanup_names[3] = { 0 };
	const struct ras_module_entry initialized = {
		.name = "alpha",
		.init = test_module_init,
		.cleanup = test_module_cleanup,
		.level = ACTIONS_MODULE,
	};
	const struct ras_module_entry following = {
		.name = "omega",
		.init = test_module_init,
		.cleanup = test_module_cleanup,
		.level = ACTIONS_MODULE,
	};
	const struct ras_module_entry preserved = {
		.name = "preserved",
		.init = test_module_init,
		.cleanup = test_module_cleanup,
		.level = ACTIONS_MODULE,
	};

	ras.name = names;
	ras.cleanup_name = cleanup_names;
	assert_int_equal(module_register(&initialized), 0);
	assert_int_equal(module_register(&failed), 0);
	assert_int_equal(module_register(&following), 0);
	assert_int_equal(module_register(&preserved), 0);
	assert_int_equal(module_init(&ras, "failed"), -EINVAL);
	assert_false(module_is_enabled("failed"));
	ctx = module_test_context("failed");
	assert_non_null(ctx);
	assert_null(ctx->ras);
	assert_null(ctx->priv);
	assert_int_equal(module_init(&ras, "preserved"), 0);
	assert_int_equal(modules_init(&ras), 0);
	assert_int_equal(ras.cleanup_count, 0);
	assert_true(module_is_enabled("alpha"));
	assert_false(module_is_enabled("failed"));
	assert_true(module_is_enabled("omega"));
	assert_true(module_is_enabled("preserved"));
	assert_null(ctx->ras);
	assert_null(ctx->priv);
	modules_unregister();
	assert_int_equal(ras.cleanup_count, 3);
	assert_string_equal(ras.cleanup_name[0], "alpha");
	assert_string_equal(ras.cleanup_name[1], "omega");
	assert_string_equal(ras.cleanup_name[2], "preserved");
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

	ras.name = names;
	assert_int_equal(module_register(&entry), 0);
	assert_false(modules_have_sql_backend());
	assert_int_equal(module_init(&ras, "test-db"), 0);
	assert_true(modules_have_sql_backend());
	assert_int_equal(module_cleanup("test-db"), 0);
	assert_false(modules_have_sql_backend());
	modules_unregister();
}

static void test_context_helpers(void **state)
{
	struct ras_events ras = { 0 };
	struct ras_events replacement = { 0 };
	struct ras_module_ctx *ctx;
	struct ras_events *old;
	const char *names[1] = { 0 };
	const char *cleanup_names[1] = { 0 };
	const struct ras_module_entry entry = {
		.name = "context",
		.level = ACTIONS_MODULE,
		.init = test_module_init,
		.cleanup = test_module_cleanup,
	};

	ras.name = names;
	ras.cleanup_name = cleanup_names;
	assert_int_equal(module_register(&entry), 0);
	assert_int_equal(modules_init(&ras), 0);
	ctx = module_test_context("context");
	assert_non_null(ctx);
	assert_ptr_equal(ctx->ras, &ras);
	old = ctx->ras;
	ctx->ras = &replacement;
	assert_ptr_equal(ctx->ras, &replacement);
	ctx->ras = old;
	assert_null(module_test_context(NULL));
	assert_null(module_test_context("missing"));
	modules_unregister();
}

static void test_test_registry(void **state)
{
	assert_true(module_test_group_is_registered(TEST_GROUP_CORE));
	assert_true(module_test_group_is_registered(TEST_GROUP_MODULES));
	assert_int_equal(module_test_register(TEST_GROUP_MAX, test_modules, 0),
			 -EINVAL);
	assert_int_equal(module_test_register(TEST_GROUP_MODULES,
					      test_modules, 0), -EEXIST);
}

static void test_init_cleanup(void **state)
{
	struct ras_events ras = { 0 };
	static const char * const init_order[] = {
		"delta", "echo", "beta", "alpha", "charlie", "foxtrot"
	};
	static const char * const cleanup_order[] = {
		"alpha", "charlie", "foxtrot", "beta", "echo", "delta"
	};
	const char *cleanup_names[ARRAY_SIZE(init_order)] = { 0 };
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
			.level  = ACTIONS_MODULE,
		}, {
			.name   = "delta",
			.init   = test_module_init,
			.cleanup = test_module_cleanup,
			.level  = DB_MODULE,
		}, {
			.name   = "foxtrot",
			.init   = test_module_init,
			.cleanup = test_module_cleanup,
			.level  = ACTIONS_MODULE,
		}, {
			.name   = "alpha",
			.init   = test_module_init,
			.cleanup = test_module_cleanup,
			.level  = ACTIONS_MODULE,
		},
	};

	ras.name = calloc(ARRAY_SIZE(mods), sizeof(const char *));
	ras.cleanup_name = cleanup_names;

	for (int i = 0; i < ARRAY_SIZE(mods); i++) {
		rc = module_register(&mods[i]);
		assert_int_equal(rc, 0);
	}

	assert_int_equal(modules_init(&ras), 0);

	for (int i = 0; i < ARRAY_SIZE(mods); i++) {
		assert_string_equal(ras.name[i], init_order[i]);
		assert_true(module_is_registered(ras.name[i]));
	}

	assert_int_equal(ras.errors, 0);

	modules_unregister();
	assert_int_equal(ras.cleanup_count, ARRAY_SIZE(mods));
	for (int i = 0; i < ras.cleanup_count; i++)
		assert_string_equal(ras.cleanup_name[i], cleanup_order[i]);
	for (int i = 0; i < ARRAY_SIZE(mods); i++)
		assert_false(module_is_registered(mods[i].name));
	free(ras.name);
	ras.name = NULL;
}

/*
 * Unit test runner
 */

static const struct CMUnitTest tests[] = {
	cmocka_unit_test(test_register_null_entry),
	cmocka_unit_test(test_register_single_module),
	cmocka_unit_test(test_register_duplicate_module),
	cmocka_unit_test(test_register_modules_in_order),
	cmocka_unit_test(test_register_mixed_order),
	cmocka_unit_test(test_register_muptiple_levels),
	cmocka_unit_test(test_named_module_lifecycle),
	cmocka_unit_test(test_failed_init_isolation),
	cmocka_unit_test(test_sql_backend_state),
	cmocka_unit_test(test_context_helpers),
	cmocka_unit_test(test_test_registry),
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

REGISTER_TEST(TEST_GROUP_MODULES, test_modules, 0);
