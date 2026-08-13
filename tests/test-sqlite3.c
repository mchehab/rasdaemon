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
	int foo;
	char *bar;
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
 * Tests
 */

static void test_db_get_sql_type(void **state)
{
	const char *type_str;
	int rc;

	rc = db_open(&backend, 0, &ras, sizeof(struct mock_priv));
	assert_int_equal(rc, 0);
	assert_non_null(ras.db);

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

	rc = db_close(0, &ras);
	assert_int_equal(rc, 0);
}

static void test_db_bind_type(void **state)
{
	struct ras_stmt *stmt;
	const char *s = "boo";
	int rc;

	rc = db_open(&backend, 0, &ras, sizeof(struct mock_priv));
	assert_int_equal(rc, 0);
	assert_non_null(ras.db);

	/* Prepare an INSERT statement */
	rc = db_create_table_prep_stmt(&ras, &stmt, NULL);
	assert_int_equal(rc, 0);
	assert_non_null(stmt);

	db_bind_type(stmt, DB_TYPE_INT32, 1, 42, -1);
	db_bind_type(stmt, DB_TYPE_TEXT, 2, (uint64_t)s, (int)strlen("s"));

	rc = db_finalize(stmt);
	assert_int_equal(rc, 0);

	rc = db_close(0, &ras);
	assert_int_equal(rc, 0);

	rc = db_close(0, &ras);
	assert_int_equal(rc, 0);
}

static void test_db_bind(void **state)
{
	struct ras_stmt *stmt;
	int rc;

	static const struct db_fields fields[] = {
		{ .name = "id",   .type = DB_TYPE_SERIAL, .is_pk = true },
		{ .name = "val",  .type = DB_TYPE_INT32,  .is_pk = false },
	};

	rc = db_open(&backend, 0, &ras, sizeof(struct mock_priv));
	assert_int_equal(rc, 0);
	assert_non_null(ras.db);

	db_create_table_prep_stmt(&ras, &stmt, NULL);
	assert_non_null(stmt);

	/* Bind a set of fields */
	db_bind(stmt, fields, 1, (uint64_t)100, -1);

	rc = db_finalize(stmt);
	assert_int_equal(rc, 0);

	rc = db_close(0, &ras);
	assert_int_equal(rc, 0);
}

static void test_db_prepare_stmt(void **state)
{
	struct ras_stmt *stmt;
	int rc;

	rc = db_open(&backend, 0, &ras, sizeof(struct mock_priv));
	assert_int_equal(rc, 0);
	assert_non_null(ras.db);

	db_create_table_prep_stmt(&ras, &stmt, NULL);
	assert_non_null(stmt);

	rc = db_prepare_stmt(NULL, &stmt, NULL);
	assert_int_equal(rc, 0);

	rc = db_finalize(stmt);
	assert_int_equal(rc, 0);

	rc = db_close(0, &ras);
	assert_int_equal(rc, 0);
}

static void test_db_create_table(void **state)
{
	struct ras_db *db;
	int rc;

	static const struct db_fields fields[] = {
		{ .name = "id",   .type = DB_TYPE_SERIAL, .is_pk = true },
		{ .name = "val",  .type = DB_TYPE_INT32,  .is_pk = false },
	};

	static const struct db_table_descriptor table_desc = {
		.name = "test_tbl",
		.fields = fields,
		.num_fields = sizeof(fields) / sizeof(fields[0]),
	};

	/* Open a database connection first */

	rc = db_open(&backend, 0, &ras, sizeof(struct mock_priv));
	assert_int_equal(rc, 0);
	assert_non_null(ras.db);

	rc = db_create_table(db, &table_desc);
	assert_int_equal(rc, 0);

	rc = db_close(0, &ras);
	assert_int_equal(rc, 0);
}

static void test_db_alter_table(void **state)
{
	struct ras_stmt *stmt;
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

	static const struct db_table_descriptor table_desc = {
		.name = "test_tbl",
		.fields = fields,
		.num_fields = ARRAY_SIZE(fields),
	};

	rc = db_open(&backend, 0, &ras, sizeof(struct mock_priv));
	assert_int_equal(rc, 0);
	assert_non_null(ras.db);

	rc = db_create_table(ras.db, &org_table_desc);
	assert_int_equal(rc, 0);

	rc = db_alter_table(ras.db, &stmt, &table_desc);
	assert_int_equal(rc, 0);

	rc = db_close(0, &ras);
	assert_int_equal(rc, 0);
}

static void test_db_eval_stmt(void **state)
{
	struct ras_stmt *stmt;
	int rc;

	static const struct db_fields fields[] = {
		{ .name = "id",   .type = DB_TYPE_SERIAL, .is_pk = true },
		{ .name = "val",  .type = DB_TYPE_INT32,  .is_pk = false },
	};
	static const struct db_table_descriptor table_desc = {
		.name = "test_tbl",
		.fields = fields,
		.num_fields = sizeof(fields) / sizeof(fields[0]),
	};

	rc = db_open(&backend, 0, &ras, sizeof(struct mock_priv));
	assert_int_equal(rc, 0);
	assert_non_null(ras.db);

	rc = db_prepare_stmt(ras.db, &stmt, &table_desc);
	assert_int_equal(rc, 0);
	assert_non_null(stmt);

	db_bind(stmt, fields, 1, (uint64_t)1, -1);
	rc = db_eval_stmt(stmt, "test_tbl");
	assert_int_equal(rc, 0);

	rc = db_finalize(stmt);
	assert_int_equal(rc, 0);

	rc = db_close(0, &ras);
	assert_int_equal(rc, 0);
}

static void test_db_cpu_finalize(void **state)
{
	struct ras_stmt *stmt;
	int rc;

	static const struct db_fields fields[] = {
		{ .name = "id",   .type = DB_TYPE_SERIAL, .is_pk = true },
		{ .name = "val",  .type = DB_TYPE_INT32,  .is_pk = false },
	};

	static const struct db_table_descriptor table_desc = {
		.name = "test_tbl",
		.fields = fields,
		.num_fields = sizeof(fields) / sizeof(fields[0]),
	};

	rc = db_open(&backend, 0, &ras, sizeof(struct mock_priv));
	assert_int_equal(rc, 0);
	assert_non_null(ras.db);

	rc = db_prepare_stmt(ras.db, &stmt, &table_desc);
	assert_int_equal(rc, 0);
	assert_non_null(stmt);

	db_bind(stmt, fields, 1, (uint64_t)99, -1);
	rc = db_cpu_finalize(0, stmt, "test_tbl");
	assert_int_equal(rc, 0);

	rc = db_close(0, &ras);
	assert_int_equal(rc, 0);
}

static void test_sqlite3_init(void **state)
{
	int rc = module_init(&ras, "db-sqlite3");
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

static void test_cleanup(void **state)
{
	modules_unregister();
	assert_null(ras_modules.head);
	assert_null(ras_modules.next);
}

/*
 * Unit test runner
 */

static const struct CMUnitTest tests[] = {
	cmocka_unit_test(test_sqlite3_init),
	cmocka_unit_test(test_db_get_sql_type),
	cmocka_unit_test(test_db_bind_type),
	cmocka_unit_test(test_db_bind),
	cmocka_unit_test(test_db_prepare_stmt),
	cmocka_unit_test(test_db_create_table),
	cmocka_unit_test(test_db_alter_table),
	cmocka_unit_test(test_db_eval_stmt),
	cmocka_unit_test(test_db_cpu_finalize),
//	cmocka_unit_test(test_db_insert_and_select),
	cmocka_unit_test(test_cleanup),
};

int test_sqlite3(void)
{
	return _cmocka_run_group_tests("sqlite3 backend",
				       tests,
				       ARRAY_SIZE(tests),
				       NULL,
				       NULL);
}
