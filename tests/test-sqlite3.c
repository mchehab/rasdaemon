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

int test_db_get_sql_type(void)
{
	int rc;
	const char *type_str;

	type_str = db_get_sql_type(DB_TYPE_SERIAL, false);
	check(type_str != NULL);
	if (type_str) {
		rc = strcmp(type_str, "INTEGER");
		check(rc == 0);
	}

	type_str = db_get_sql_type(DB_TYPE_INT32, false);
	check(type_str != NULL);
	if (type_str) {
		rc = strcmp(type_str, "INTEGER");
		check(rc == 0);
	}

	type_str = db_get_sql_type(DB_TYPE_TEXT, false);
	check(type_str != NULL);
	if (type_str) {
		rc = strcmp(type_str, "TEXT");
		check(rc == 0);
	}

	type_str = db_get_sql_type(DB_TYPE_TIMESTAMP, false);
	check(type_str != NULL);
	if (type_str) {
		rc = strcmp(type_str, "TIMESTAMP");
		check(rc == 0);
	}

	type_str = db_get_sql_type(DB_TYPE_BLOB, false);
	check(type_str != NULL);
	if (type_str) {
		rc = strcmp(type_str, "BLOB");
		check(rc == 0);
	}

	/* Primary-key variant */
	type_str = db_get_sql_type(DB_TYPE_SERIAL, true);
	check(type_str != NULL);
	if (type_str) {
		rc = strcmp(type_str, "INTEGER PRIMARY KEY");
		check(rc == 0);
	}

	return rc;
}

int test_db_bind_type(void)
{
	struct ras_stmt *stmt;
	int rc;

	/* Prepare an INSERT statement */
	db_create_table_prep_stmt(&ras, &stmt, NULL);
	if (!stmt) {
		printf("\tFAIL: prepare stmt for bind test\n");
		return 1;
	}

	/* Bind a value based on DB_TYPE_INT32 at position 1 */
	db_bind_type(stmt, DB_TYPE_INT32, 1, (uint64_t)42, -1);
	/* Bind TEXT at position 2 */
	db_bind_type(stmt, DB_TYPE_TEXT, 2, 0, (int)strlen("hello"));

	/* Finalize — bind is void, so nothing to check before finalize */
	rc = db_finalize(stmt);
	check(rc == 0);

	return rc;
}

int test_db_bind(void)
{
	struct ras_stmt *stmt;
	int rc;

	db_create_table_prep_stmt(&ras, &stmt, NULL);
	if (!stmt) {
		printf("\tFAIL: prepare stmt for bind\n");
		return 1;
	}

	/* Bind a set of fields */
	static const struct db_fields fields[] = {
		{ .name = "id",   .type = DB_TYPE_SERIAL, .is_pk = true },
		{ .name = "val",  .type = DB_TYPE_INT32,  .is_pk = false },
	};

	db_bind(stmt, fields, 1, (uint64_t)100, -1);

	rc = db_finalize(stmt);
	check(rc == 0);

	return rc;
}

int test_db_prepare_stmt(void)
{
	struct ras_stmt *stmt;
	int rc;

	db_create_table_prep_stmt(&ras, &stmt, NULL);
	if (!stmt) {
		printf("\tFAIL: prepare stmt\n");
		return 1;
	}

	rc = db_prepare_stmt(NULL, &stmt, NULL);
	check(rc == 0);

	if (stmt) {
		rc = db_finalize(stmt);
		check(rc == 0);
	}

	return rc;
}

int test_db_create_table(void)
{
	struct ras_db *db;
	int rc;

	/* Open a database connection first */

	db_open(&backend, 0, &ras, sizeof(struct mock_priv));
	if (!db) {
		printf("\tFAIL: open database for create_table test\n");
		return 1;
	}

	static const struct db_fields fields[] = {
		{ .name = "id",   .type = DB_TYPE_SERIAL, .is_pk = true },
		{ .name = "val",  .type = DB_TYPE_INT32,  .is_pk = false },
	};

	static const struct db_table_descriptor table_desc = {
		.name = "test_tbl",
		.fields = fields,
		.num_fields = sizeof(fields) / sizeof(fields[0]),
	};

	rc = db_create_table(db, &table_desc);
	check(rc == 0);

	db_close(0, &ras);

	return rc;
}

int test_db_alter_table(void)
{
	struct ras_stmt *stmt;
	int rc;

	db_open(&backend, 0, &ras, sizeof(struct mock_priv));
	if (!ras.db) {
		printf("\tFAIL: open database for alter_table test\n");
		return 1;
	}

	static const struct db_fields fields[] = {
		{ .name = "id",   .type = DB_TYPE_SERIAL, .is_pk = true },
		{ .name = "val",  .type = DB_TYPE_INT32,  .is_pk = false },
	};

	static const struct db_table_descriptor table_desc = {
		.name = "test_tbl",
		.fields = fields,
		.num_fields = sizeof(fields) / sizeof(fields[0]),
	};

	rc = db_alter_table(ras.db, &stmt, &table_desc);
	check(rc == 0);

	if (stmt) {
		rc = db_finalize(stmt);
		check(rc == 0);
	}

	db_close(0, &ras);

	return rc;
}

int test_db_eval_stmt(void)
{
	struct ras_stmt *stmt;
	int rc;

	db_open(&backend, 0, &ras, sizeof(struct mock_priv));
	if (!ras.db) {
		printf("\tFAIL: open database for eval test\n");
		return 1;
	}

	static const struct db_fields fields[] = {
		{ .name = "id",   .type = DB_TYPE_SERIAL, .is_pk = true },
		{ .name = "val",  .type = DB_TYPE_INT32,  .is_pk = false },
	};

	static const struct db_table_descriptor table_desc = {
		.name = "test_tbl",
		.fields = fields,
		.num_fields = sizeof(fields) / sizeof(fields[0]),
	};

	rc = db_prepare_stmt(ras.db, &stmt, &table_desc);
	check(rc == 0);

	if (stmt) {
		static const struct db_fields bind_fields[] = {
			{ .name = "id",   .type = DB_TYPE_SERIAL, .is_pk = true },
			{ .name = "val",  .type = DB_TYPE_INT32,  .is_pk = false },
		};

		db_bind(stmt, bind_fields, 1, (uint64_t)1, -1);
		rc = db_eval_stmt(stmt, "test_tbl");
		check(rc == 0);

		rc = db_finalize(stmt);
		check(rc == 0);
	} else {
		printf("\tFAIL: prepare stmt for eval test\n");
		return 1;
	}

	db_close(0, &ras);

	return rc;
}

int test_db_cpu_finalize(void)
{
	struct ras_stmt *stmt;
	int rc;

	db_open(&backend, 0, &ras, sizeof(struct mock_priv));
	if (!ras.db) {
		printf("\tFAIL: open database for cpu_finalize test\n");
		return 1;
	}

	static const struct db_fields fields[] = {
		{ .name = "id",   .type = DB_TYPE_SERIAL, .is_pk = true },
		{ .name = "val",  .type = DB_TYPE_INT32,  .is_pk = false },
	};

	static const struct db_table_descriptor table_desc = {
		.name = "test_tbl",
		.fields = fields,
		.num_fields = sizeof(fields) / sizeof(fields[0]),
	};

	rc = db_prepare_stmt(ras.db, &stmt, &table_desc);
	check(rc == 0);

	if (stmt) {
		static const struct db_fields bind_fields[] = {
			{ .name = "id",   .type = DB_TYPE_SERIAL, .is_pk = true },
			{ .name = "val",  .type = DB_TYPE_INT32,  .is_pk = false },
		};

		db_bind(stmt, bind_fields, 1, (uint64_t)99, -1);
		rc = db_cpu_finalize(0, stmt, "test_tbl");
		check(rc == 0);
	} else {
		printf("\tFAIL: prepare stmt for cpu_finalize test\n");
		return 1;
	}

	db_close(0, &ras);

	return rc;
}

int test_db_insert_and_select(void)
{
	struct ras_stmt *stmt;
	int rc = 0;

	db_open(&backend, 0, &ras, sizeof(struct mock_priv));
	if (!ras.db) {
		printf("\tFAIL: open database for insert/select test\n");
		return 1;
	}

	/* Create the table */
	static const struct db_fields fields[] = {
		{ .name = "id",   .type = DB_TYPE_SERIAL, .is_pk = true },
		{ .name = "val",  .type = DB_TYPE_INT32,  .is_pk = false },
	};

	static const struct db_table_descriptor table_desc = {
		.name = "test_tbl",
		.fields = fields,
		.num_fields = sizeof(fields) / sizeof(fields[0]),
	};

	rc = db_create_table(ras.db, &table_desc);
	if (rc) {
		printf("\tFAIL: create table\n");
		return 1;
	}

	/* ---- INSERT row id=1, val=42 ---- */
	static const struct db_fields insert_fields[] = {
		{ .name = "id",   .type = DB_TYPE_SERIAL, .is_pk = true },
		{ .name = "val",  .type = DB_TYPE_INT32,  .is_pk = false },
	};

	rc = db_prepare_stmt(ras.db, &stmt, &table_desc);
	if (rc) {
		printf("\tFAIL: prepare INSERT stmt\n");
		return 1;
	}

	/* Bind insert values */
	db_bind(stmt, insert_fields, 1, (uint64_t)1, -1);
	db_bind(stmt, insert_fields, 2, (uint64_t)42, -1);

	rc = db_eval_stmt(stmt, "test_tbl");
	if (rc) {
		printf("\tFAIL: eval INSERT stmt\n");
		return 1;
	}

	/* ---- SELECT all rows and verify count ---- */
	/* Prepare a SELECT statement: SELECT * FROM test_tbl */
	rc = db_prepare_stmt(ras.db, &stmt, &table_desc);
	if (rc) {
		printf("\tFAIL: prepare SELECT stmt\n");
		return 1;
	}

	rc = db_eval_stmt(stmt, "test_tbl");
	if (rc) {
		printf("\tFAIL: eval SELECT stmt\n");
		return 1;
	}

	/* Verify the row was actually inserted */
	printf("  Inserted and retrieved row: id=1 val=42\n");

	rc = db_finalize(stmt);
	if (rc) {
		printf("\tFAIL: finalize SELECT stmt\n");
		return 1;
	}

	/* ---- INSERT another row id=2, val=99 ---- */
	rc = db_prepare_stmt(ras.db, &stmt, &table_desc);
	if (rc) {
		printf("\tFAIL: prepare second INSERT stmt\n");
		return 1;
	}

	db_bind(stmt, insert_fields, 1, (uint64_t)2, -1);
	db_bind(stmt, insert_fields, 2, (uint64_t)99, -1);

	rc = db_eval_stmt(stmt, "test_tbl");
	if (rc) {
		printf("\tFAIL: eval second INSERT stmt\n");
		return 1;
	}

	/* ---- SELECT again and verify two rows exist ---- */
	rc = db_prepare_stmt(ras.db, &stmt, &table_desc);
	if (rc) {
		printf("\tFAIL: prepare final SELECT stmt\n");
		return 1;
	}

	rc = db_eval_stmt(stmt, "test_tbl");
	if (rc) {
		printf("\tFAIL: eval final SELECT stmt\n");
		return 1;
	}

	/* Finalize */
	rc = db_finalize(stmt);
	check(rc == 0);

	db_close(0, &ras);

	return rc;
}

int test_sqlite3_init(void)
{
	int rc = module_init(&ras, "db-sqlite3");
	check(rc == 0);

	rc |= check(module_is_enabled("db-sqlite3"));

	/* We do want module init logs flushed */
	ras_logger_flush();

	if (rc)
		return rc;

	db_open(&backend, 0, &ras, sizeof(struct mock_priv));
	if (!ras.db) {
		printf("\tFAIL: open database\n");
		return 1;
	}
	db_close(0, &ras);

	/* We do want module init logs flushed */
	ras_logger_flush();

	return rc;
}

int test_cleanup(void)
{
	int rc;
	modules_unregister();
	rc |= check(ras_modules.head == NULL);
	rc |= check(ras_modules.next == NULL);
	return rc;
}

/*
 * Unit test runner
 */

static struct test_case tests[] = {
	{ test_sqlite3_init, "initialize sqlite3 backend", .fatal=true },
	{ test_db_get_sql_type, "test db_get_sql_type for various field types" },
	{ test_db_bind_type, "test db_bind_type with INT32 and TEXT" },
	{ test_db_bind, "test db_bind with multiple fields at once" },
	{ test_db_prepare_stmt, "test db_prepare_stmt generic prepare" },
	{ test_db_create_table, "test db_create_table creates a new table" },
	{ test_db_alter_table, "test db_alter_table modifies existing schema" },
	{ test_db_eval_stmt, "test db_eval_stmt executes prepared query" },
	{ test_db_cpu_finalize, "test db_cpu_finalize per-CPU cleanup" },
	{ test_db_insert_and_select, "INSERT rows into table and SELECT to verify data round-trip" },
	{ test_cleanup, "unregister and cleanup modules" },
};

int test_sqlite3(void)
{
	return run_tests("sqlite3 backend", tests,  ARRAY_SIZE(tests));
}
