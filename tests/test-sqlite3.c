// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2026 Mauro Carvalho Chehab <mchehab+huawei@kernel.org>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/* for sqlite3 flags */
#include <sqlite3.h>

#include "config.h"

#include "core/ras-events.h"
#include "core/ras-logger.h"
#include "core/modules.h"

#include "db/ras-db.h"
#include "db/db-sqlite3.h"

#include "tests/unittest.h"

extern struct module_list ras_modules;

struct ras_events ras = { 0 };

struct mock_priv {
	struct ras_stmt *stmt;
};

static struct db_sqlite3_conn_params conn_parms = {
	.dir = "/tmp",
	.fname = "sqlite3_mock.db",
	.extra_flags = SQLITE_OPEN_MEMORY,
};

struct db_backend backend = {
	.name = "sqlite3",
	.conn_parms = &conn_parms,
};

/*
 * Ancillary function to access sqlite3 database directly
 */

struct db_values {
	enum db_field_type	type;
	uint64_t		value;
	char			*string;
};

static void sqlite3_compare_value(void **state,
				  int pos,
				  const struct db_values *expected,
				  sqlite3_stmt *stmt)
{
	int type = sqlite3_column_type(stmt, pos);
	const char *str;
	int exp_type;
	uint64_t val;

	switch (expected->type) {
	case DB_TYPE_SERIAL:
	case DB_TYPE_INT64:
	case DB_TYPE_INT32:
		exp_type = SQLITE_INTEGER;
		break;
	case DB_TYPE_TIMESTAMP:
	case DB_TYPE_TEXT:
		exp_type = SQLITE_TEXT;
	case DB_TYPE_BLOB:
	default:
		exp_type = SQLITE_BLOB;
	}

	switch (exp_type) {
	case SQLITE_INTEGER:
		val = sqlite3_column_int64(stmt, pos);

		assert_int_equal(type, exp_type);
		assert_int_equal(val, (int64_t)expected->value);
		return;
	case SQLITE_TEXT:
		if (type == SQLITE_NULL)
			str = NULL;
		else
			str = (const char *)sqlite3_column_text(stmt, pos);
		break;

	case SQLITE_BLOB:
	default:
		if (type == SQLITE_NULL)
			str = NULL;
		else
			str = (const char *)sqlite3_column_blob(stmt, pos);
	}

	printf("pos %d, type %d, str: %p, expected: %p\n", pos, type, str, expected->string);

	if (!expected->string) {
		assert_null(str);
		return;
	}

	assert_non_null(str);
	assert_string_equal(str, expected->string);
}

static int sqlite3_check_values(void **state,
				struct ras_db *__db,
				struct ras_stmt **__stmt,
				const struct db_table_descriptor *db_tab,
				const struct db_values *values, int len) {
	sqlite3 *db = (void *)__db;
	int i, rc, nrow = 0;
	sqlite3_stmt **stmt = (void *)__stmt;
	char query[1024];

	snprintf(query, sizeof(query), "SELECT * from %s", db_tab->name);

	rc = sqlite3_prepare_v2(db, query, -1, stmt, NULL);
	assert_int_equal(rc, 0);

	while (sqlite3_step(*stmt) == SQLITE_ROW) {
		nrow++;
		for (i = 0; i < len; i++) {
			int pos = sqlite3_column_count(*stmt);

			assert_true(i < pos);

			sqlite3_compare_value(state, i, &values[i], *stmt);
		}
		assert_true(i == len);
	}

	rc = db_finalize(*__stmt);
	assert_int_equal(rc, 0);

	/*
	 * NOTE: The logic assumes just one row at the table.
	 *	 This should be enough to check if Prepare stmt work.
	 */
	assert_int_equal(nrow, 1);

	if (rc == SQLITE_DONE)
		return 0;

	return rc;
}

/*
 * Tests
 */

/* Check if type mapping is done the right way */
static void test_db_get_sql_type(void **state)
{
	const char *type_str;
	int rc;

	type_str = db_get_sql_type(DB_TYPE_SERIAL, false);
	assert_string_equal(type_str, "INTEGER");

	type_str = db_get_sql_type(DB_TYPE_INT32, false);
	assert_string_equal(type_str, "INTEGER");

	type_str = db_get_sql_type(DB_TYPE_INT64, false);
	assert_string_equal(type_str, "INTEGER");

	type_str = db_get_sql_type(DB_TYPE_TEXT, false);
	assert_string_equal(type_str, "TEXT");

	type_str = db_get_sql_type(DB_TYPE_TIMESTAMP, false);
	assert_string_equal(type_str, "TEXT");

	type_str = db_get_sql_type(DB_TYPE_BLOB, false);
	assert_string_equal(type_str, "BLOB");

	/* Primary key variants for integer types */

	type_str = db_get_sql_type(DB_TYPE_SERIAL, true);
	assert_string_equal(type_str, "INTEGER PRIMARY KEY");

	type_str = db_get_sql_type(DB_TYPE_INT32, true);
	assert_string_equal(type_str, "INTEGER PRIMARY KEY");

	type_str = db_get_sql_type(DB_TYPE_INT64, true);
	assert_string_equal(type_str, "INTEGER PRIMARY KEY");
}

/* Call db_prepare_insert_stmt without binding first */
static void test_db_no_binding(void **state)
{
	struct mock_priv *priv = ras.db_priv;
	struct ras_stmt *stmt = priv->stmt;
	int rc;

	static const struct db_fields fields[] = {
		{ .name = "id",   .type = DB_TYPE_SERIAL,    .is_pk = true },
		{ .name = "val",  .type = DB_TYPE_TEXT       },
		{ .name = "time",  .type = DB_TYPE_TIMESTAMP },
	};

	static const struct db_table_descriptor db_tab = {
		.name = "test_tbl",
		.fields = fields,
		.num_fields = sizeof(fields) / sizeof(fields[0]),
	};
	static struct db_values vals[] = {
		{
			.type = fields[0].type,
			.value = 1,
		},
		{
			.type = fields[1].type,
			.string = NULL,
		},
	};

	db_create_table(ras.db, &db_tab);
	assert_non_null(stmt);

	rc = db_prepare_insert_stmt(ras.db, &stmt, &db_tab);
	assert_int_equal(rc, 0);

	/* No bindings here */

	rc = db_eval_stmt(stmt, db_tab.name);
	assert_int_equal(rc, 0);

	rc = db_finalize(stmt);
	assert_int_equal(rc, 0);

	rc = sqlite3_check_values(state, ras.db, &stmt, &db_tab,
				  vals, ARRAY_SIZE(vals));
}

/* check if db_bind_type is working */
static void test_db_bind_type(void **state)
{
	struct mock_priv *priv = ras.db_priv;
	struct ras_stmt *stmt = priv->stmt;
	int rc, pos = 1;

	static const struct db_fields fields[] = {
		{ .name = "id",    .type = DB_TYPE_INT32,    .is_pk = true },
		{ .name = "str",   .type = DB_TYPE_TEXT  },
		{ .name = "int64", .type = DB_TYPE_INT64 },
	};

	static const struct db_table_descriptor db_tab = {
		.name = "test_tbl",
		.fields = fields,
		.num_fields = sizeof(fields) / sizeof(fields[0]),
	};

	static struct db_values vals[] = {
		{
			.type = fields[0].type,
			.value = 1,
		},
		{
			.type = fields[1].type,
			.string = "boo",
		},
		{
			.type = fields[2].type,
			.value = -1L,
		},
	};

	/* Prepare an INSERT statement */
	rc = db_create_table_prep_stmt(&ras, &stmt, &db_tab);
	assert_int_equal(rc, 0);
	assert_non_null(stmt);

	db_bind_type(stmt, vals[0].type, pos++, (uint64_t)vals[0].value, -1);
	db_bind_type(stmt, vals[1].type, pos++, (uint64_t)vals[1].string, -1);
	db_bind_type(stmt, vals[2].type, pos++, (uint64_t)vals[2].value, -1);

	rc = db_eval_stmt(stmt, db_tab.name);
	assert_int_equal(rc, 0);

	rc = db_finalize(stmt);
	assert_int_equal(rc, 0);

	rc = sqlite3_check_values(state, ras.db, &stmt, &db_tab,
				  vals, ARRAY_SIZE(vals));
	assert_int_equal(rc, 0);
}

/* check if db_bind is working */
static void test_db_bind(void **state)
{
	struct mock_priv *priv = ras.db_priv;
	struct ras_stmt *stmt = priv->stmt;
	int rc, pos = 1;

	static const struct db_fields fields[] = {
		{ .name = "id",   .type = DB_TYPE_SERIAL, .is_pk = true },
		{ .name = "text1", .type = DB_TYPE_TEXT },
		{ .name = "text2", .type = DB_TYPE_TEXT },
	};

	static const struct db_table_descriptor db_tab = {
		.name = "test_tbl",
		.fields = fields,
		.num_fields = sizeof(fields) / sizeof(fields[0]),
	};
	static struct db_values vals[] = {
		{
			.type = fields[0].type,
			.value = 1,
		},
		{
			.type = fields[1].type,
			.string = "The quick brown fox",
		},
		{
			.type = fields[2].type,
			.string = "jumps over the lazy dog",
		},
	};

	db_create_table_prep_stmt(&ras, &stmt, &db_tab);
	assert_non_null(stmt);

	db_bind(stmt, fields, pos++, (uint64_t)vals[1].string, -1);
	db_bind(stmt, fields, pos++, (uint64_t)vals[2].string, -1);

	rc = db_eval_stmt(stmt, db_tab.name);
	assert_int_equal(rc, 0);

	rc = sqlite3_check_values(state, ras.db, &stmt, &db_tab,
				  vals, ARRAY_SIZE(vals));

	rc = db_finalize(stmt);
	assert_int_equal(rc, 0);
}

/* Test table creation */
static void test_db_create_table(void **state)
{
	int rc;

	static const struct db_fields fields[] = {
		{ .name = "id",   .type = DB_TYPE_SERIAL, .is_pk = true },
		{ .name = "val",  .type = DB_TYPE_INT32,  .is_pk = false },
	};

	static const struct db_table_descriptor db_tab = {
		.name = "test_tbl",
		.fields = fields,
		.num_fields = sizeof(fields) / sizeof(fields[0]),
	};

	/* Open a database connection first */

	rc = db_create_table(ras.db, &db_tab);
	assert_int_equal(rc, 0);
}

/* Test changing a table definition */
static void test_db_alter_table(void **state)
{
	struct mock_priv *priv = ras.db_priv;
	struct ras_stmt *stmt = priv->stmt;
	int rc;

	static const struct db_fields org_fields[] = {
		{ .name = "id",   .type = DB_TYPE_SERIAL, .is_pk = true },
		{ .name = "val",  .type = DB_TYPE_INT32,  .is_pk = false },
	};

	static const struct db_table_descriptor org_table_desc = {
		.name = "test_tbl",
		.fields = org_fields,
		.num_fields = ARRAY_SIZE(org_fields),
	};

	static const struct db_fields fields[] = {
		{ .name = "id",   .type = DB_TYPE_SERIAL, .is_pk = true },
		{ .name = "val",  .type = DB_TYPE_INT64,  .is_pk = false },
	};

	static const struct db_table_descriptor db_tab = {
		.name = "test_tbl",
		.fields = fields,
		.num_fields = ARRAY_SIZE(fields),
	};

	rc = db_create_table(ras.db, &org_table_desc);
	assert_int_equal(rc, 0);
}

static void test_db_cpu_finalize(void **state)
{
	struct mock_priv *priv = ras.db_priv;
	struct ras_stmt *stmt = priv->stmt;
	int rc;

	static const struct db_fields fields[] = {
		{ .name = "id",   .type = DB_TYPE_SERIAL, .is_pk = true },
		{ .name = "val",  .type = DB_TYPE_INT32,  .is_pk = false },
	};

	static const struct db_table_descriptor db_tab = {
		.name = "test_tbl",
		.fields = fields,
		.num_fields = sizeof(fields) / sizeof(fields[0]),
	};
	static struct db_values vals[] = {
		{
			.type = fields[0].type,
			.value = 1,
		},
		{
			.type = fields[1].type,
			.value = 99,
		},
	};

	rc = db_prepare_insert_stmt(ras.db, &stmt, &db_tab);
	assert_int_equal(rc, 0);
	assert_non_null(stmt);

	db_bind(stmt, fields, 1, vals[1].value, -1);

	rc = db_eval_stmt(stmt, db_tab.name);
	assert_int_equal(rc, 0);

	rc = db_cpu_finalize(0, stmt, db_tab.name);
	assert_int_equal(rc, 0);

	rc = sqlite3_check_values(state, ras.db, &stmt, &db_tab,
				  vals, ARRAY_SIZE(vals));
	assert_int_equal(rc, 0);
}

/* Check if enabling db-sqlite3 module is working */
static void test_sqlite3_init(void **state)
{
	struct mock_priv *priv;
	int rc;

	rc = module_init(&ras, "db-sqlite3");
	assert_int_equal(rc, 0);

	rc = module_is_enabled("db-sqlite3");
	if (!rc) {
		const char *msg = "Module db-sqlite3 is not enabled";
		assert_null_msg(msg, msg);
	}

	/* We do want module init logs flushed */
	ras_logger_flush();

	rc = db_open(&backend, 0, &ras, sizeof(struct mock_priv));
	assert_int_equal(rc, 0);
	assert_non_null(ras.db);

	rc = db_close(0, &ras);
	assert_int_equal(rc, 0);

	/* We do want module init logs flushed */
	ras_logger_flush();
}

/* Check if enabling db-sqlite3 unregister is working */
static void test_cleanup(void **state)
{
	modules_unregister();
	assert_null(ras_modules.head);
	assert_null(ras_modules.next);
}

/*
 * Unit test runner
 */

static int tests_setup(void **state)
{
	int rc = db_open(&backend, 0, &ras, sizeof(struct mock_priv));
	assert_int_equal(rc, 0);
	assert_non_null(ras.db);

	db_exec_sql(ras.db, "DROP TABLE IF EXISTS test_tbl");

	return rc;
}

static int tests_teardown(void **state)
{
	struct mock_priv *priv = ras.db_priv;
	int rc;

	rc = db_finalize(priv->stmt);
	assert_int_equal(rc, 0);

	rc = db_close(0, &ras);
	assert_int_equal(rc, 0);

	return rc;
}

static const struct CMUnitTest tests[] = {
	cmocka_unit_test(test_sqlite3_init),

	cmocka_unit_test_setup_teardown(test_db_get_sql_type,
					tests_setup, tests_teardown),

	cmocka_unit_test_setup_teardown(test_db_create_table,
					tests_setup, tests_teardown),

	cmocka_unit_test_setup_teardown(test_db_no_binding,
					tests_setup, tests_teardown),

	cmocka_unit_test_setup_teardown(test_db_bind_type,
					tests_setup, tests_teardown),
	cmocka_unit_test_setup_teardown(test_db_bind,
					tests_setup, tests_teardown),

	cmocka_unit_test_setup_teardown(test_db_alter_table,
					tests_setup, tests_teardown),

	cmocka_unit_test_setup_teardown(test_db_cpu_finalize,
					tests_setup, tests_teardown),

	cmocka_unit_test(test_cleanup),
};

static int group_setup(void **state)
{
	return module_init(&ras, "db-sqlite3");
}

static int group_teardown(void **state)
{
	modules_unregister();
	return 0;
}

int test_sqlite3(void)
{
	return _cmocka_run_group_tests("sqlite3 backend",
				       tests,
				       ARRAY_SIZE(tests),
				       group_setup,
				       group_teardown);
}
