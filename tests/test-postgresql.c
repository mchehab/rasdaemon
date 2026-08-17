// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2026 Mauro Carvalho Chehab <mchehab+huawei@kernel.org>
 *
 * Unit tests for the PostgreSQL backend.
 *
 * Environment variables (all optional, with sensible defaults):
 *   RAS_PG_HOST		(default: ""  = local socket)
 *   RAS_PG_PORT		(default: 5432)
 *   RAS_PG_USER		(default: "rasdaemon")
 *   RAS_PG_PASSWORD	(default: "")
 *   RAS_PG_DATABASE	(default: "rasdaemon_test")
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include <libpq-fe.h>

#include "config.h"

#include "core/ras-events.h"
#include "core/ras-logger.h"
#include "core/modules.h"

#include "db/ras-db.h"
#include "db/db-postgresql.h"
#include "db/db-postgresql-priv.h"

#include "tests/unittest.h"

extern struct module_list ras_modules;

static struct ras_events ras = { 0 };

struct mock_priv {
	struct ras_stmt *stmt;
};

static const char *env_or(const char *name, const char *def)
{
	const char *v = getenv(name);

	return (v && v[0]) ? v : def;
}

static struct db_postgresql_conn_params conn_parms = {
	.host	  = NULL,
	.port	  = 5432,
	.user	  = "rasdaemon",
	.password  = "",
	.database  = "rasdaemon_test",
	.use_ssl   = false,
	.sslmode   = NULL,
	.connect_timeout = 10,
};

static struct db_backend backend = {
	.name = "postgresql",
	.conn_parms = &conn_parms,
};


struct db_values {
	enum db_field_type	type;
	uint64_t		value;
	char			*string;
};

static char *datetime_to_iso(const char *cell)
{
	struct tm tm = {0};
	char buffer[64];
	size_t length;
	char *output;
	char *end;

	end = strptime(cell, "%Y-%m-%d %H:%M:%S", &tm);
	if (!end )
		return "";

	tm.tm_isdst = 0;

	length = strftime(buffer, sizeof(buffer),
			  "%Y-%m-%d %H:%M:%S %z", &tm);
	if (!length)
		return NULL;

	output = malloc(length + 1);
	if (output == NULL)
		return NULL;

	memcpy(output, buffer, length + 1);
	return output;
}

static int pg_check_values(void **state,
			   struct ras_db *__db,
			   struct ras_stmt **__stmt,
			   const struct db_table_descriptor *db_tab,
			   const struct db_values *values, int len)
{
	struct pg_conn_priv *conn_priv = (void *)__db;
	PGconn *db = conn_priv->conn;
	int i, ncol, nrow = 0;
	unsigned char *str;
	size_t decoded_len;
	PGresult *res;
	char buf[1024];

	snprintf(buf, sizeof(buf), "SELECT * FROM %s", db_tab->name);

	res = PQexec(db, buf);
	if (!res || PQresultStatus(res) != PGRES_TUPLES_OK) {
		fprintf(stderr, "pg_check_values: query failed\n");
		if (res)
			PQclear(res);
		return -1;
	}

	ncol = PQnfields(res);
	nrow = PQntuples(res);

	for (int r = 0; r < nrow; r++) {
		for (i = 0; i < len && i < ncol; i++) {
			const char *cell = PQgetvalue(res, r, i);
			const struct db_values *ev = &values[i];
			bool isnull = (PQgetisnull(res, r, i) != 0);

			switch (ev->type) {
			case DB_TYPE_SERIAL:
			case DB_TYPE_INT32:
			case DB_TYPE_INT64:
				{
					long long val = strtoll(cell, NULL, 10);

					assert_int_equal((long long)val,
							(long long)ev->value);
				}
				break;
			case DB_TYPE_TIMESTAMP:
				cell = datetime_to_iso(cell);
				/* fail-through */
			case DB_TYPE_TEXT:
			case DB_TYPE_BLOB:
			default:
				if (!ev->string) {
					assert_true(isnull);
				} else {
					assert_false(isnull);

					if (ev->type == DB_TYPE_BLOB) {
						str = PQunescapeBytea((unsigned char *)cell,
								      &decoded_len);

						memcpy(buf, str, decoded_len);
						buf[decoded_len] = '\0';

						assert_string_equal(buf,
								    ev->string);

					} else {
						assert_string_equal(cell, ev->string);
					}
				}
				break;
			}
		}
		assert_int_equal(i, len);
	}

	PQclear(res);
	assert_int_equal(nrow, 1);
	return 0;
}

static void test_db_get_sql_type(void **state)
{
	const char *type_str;

	type_str = db_get_sql_type(DB_TYPE_SERIAL, false);
	assert_string_equal(type_str, "BIGINT");

	type_str = db_get_sql_type(DB_TYPE_INT32, false);
	assert_string_equal(type_str, "INTEGER");

	type_str = db_get_sql_type(DB_TYPE_INT64, false);
	assert_string_equal(type_str, "BIGINT");

	type_str = db_get_sql_type(DB_TYPE_TEXT, false);
	assert_string_equal(type_str, "TEXT");

	type_str = db_get_sql_type(DB_TYPE_TIMESTAMP, false);
	assert_string_equal(type_str, "TIMESTAMPTZ");

	type_str = db_get_sql_type(DB_TYPE_BLOB, false);
	assert_string_equal(type_str, "BYTEA");

	type_str = db_get_sql_type(DB_TYPE_SERIAL, true);
	assert_string_equal(type_str, "BIGSERIAL");
}

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

	rc = db_create_table(ras.db, &db_tab);
	assert_int_equal(rc, 0);
}

static void test_db_bind_type(void **state)
{
	struct ras_stmt *stmt = NULL;
	int rc, pos = 1;

	static const struct db_fields fields[] = {
		{ .name = "id",	.type = DB_TYPE_INT32,	.is_pk = true },
		{ .name = "int64", .type = DB_TYPE_INT64  },
		{ .name = "str",   .type = DB_TYPE_TEXT   },
	};

	static const struct db_table_descriptor db_tab = {
		.name = "test_tbl",
		.fields = fields,
		.num_fields = sizeof(fields) / sizeof(fields[0]),
	};

	static struct db_values vals[] = {
		{ .type = fields[0].type, .value = 1 },
		{ .type = fields[1].type, .value = -1L },
		{ .type = fields[2].type, .string = "boo" },
	};

	rc = db_create_table_prep_stmt(&ras, &stmt, &db_tab);
	assert_int_equal(rc, 0);
	assert_non_null(stmt);

	db_bind_type(stmt, vals[0].type, pos++, (uint64_t)vals[0].value, -1);
	db_bind_type(stmt, vals[1].type, pos++, (uint64_t)vals[1].value, -1);
	db_bind_type(stmt, vals[2].type, pos++, (uint64_t)vals[2].string, -1);

	rc = db_eval_stmt(stmt, db_tab.name);
	assert_int_equal(rc, 0);

	rc = db_finalize(stmt);
	assert_int_equal(rc, 0);

	rc = pg_check_values(state, ras.db, &stmt, &db_tab,
			     vals, ARRAY_SIZE(vals));
	assert_int_equal(rc, 0);
}

static void test_db_bind(void **state)
{
	struct ras_stmt *stmt = NULL;
	int rc, pos = 1;

	static const struct db_fields fields[] = {
		{ .name = "id",   .type = DB_TYPE_SERIAL, .is_pk = true },
		{ .name = "text1", .type = DB_TYPE_TEXT   },
		{ .name = "text2", .type = DB_TYPE_TEXT   },
	};

	static const struct db_table_descriptor db_tab = {
		.name = "test_tbl",
		.fields = fields,
		.num_fields = sizeof(fields) / sizeof(fields[0]),
	};

	static struct db_values vals[] = {
		{ .type = fields[0].type, .value = 1 },
		{ .type = fields[1].type,
		  .string = "The quick brown fox" },
		{ .type = fields[2].type,
		  .string = "jumps over the lazy dog" },
	};

	rc = db_create_table_prep_stmt(&ras, &stmt, &db_tab);
	assert_int_equal(rc, 0);
	assert_non_null(stmt);

	db_bind(&db_tab, stmt, pos++, (uint64_t)vals[1].string, -1);
	db_bind(&db_tab, stmt, pos++, (uint64_t)vals[2].string, -1);

	rc = db_eval_stmt(stmt, db_tab.name);
	assert_int_equal(rc, 0);

	rc = db_finalize(stmt);
	assert_int_equal(rc, 0);

	rc = pg_check_values(state, ras.db, &stmt, &db_tab,
			     vals, ARRAY_SIZE(vals));
	assert_int_equal(rc, 0);
}

static void test_db_complex_table(void **state)
{
	struct ras_stmt *stmt = NULL;
	int rc, pos = 1;
	struct tm tm;
	char buf[64];

	static const struct db_fields fields[] = {
		{ .name = "timestamp",	.type = DB_TYPE_TIMESTAMP },
		{ .name = "id",		.type = DB_TYPE_SERIAL,   .is_pk = true },
		{ .name = "sec_type",	.type = DB_TYPE_BLOB      },
		{ .name = "severity",	.type = DB_TYPE_TEXT      },
		{ .name = "error",	.type = DB_TYPE_BLOB	  },
	};

	static const struct db_table_descriptor db_tab = {
		.name = "test_tbl",
		.fields = fields,
		.num_fields = sizeof(fields) / sizeof(fields[0]),
	};

	struct db_values vals[] = {
		{ .type = fields[0].type, .string = buf     },
		{ .type = fields[1].type, .value = 1        },
		{ .type = fields[2].type, .string = "blob1" },
		{ .type = fields[3].type, .string = "text"  },
		{ .type = fields[4].type, .string = "blob2" },
	};

	time_t now = time(NULL);
	localtime_r(&now, &tm);

	strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S %z", &tm);

	rc = db_create_table_prep_stmt(&ras, &stmt, &db_tab);
	assert_int_equal(rc, 0);
	assert_non_null(stmt);

	db_bind(&db_tab, stmt, pos++, (uint64_t)vals[0].string, -1);
	db_bind(&db_tab, stmt, pos++, (uint64_t)vals[2].string, -1);
	db_bind(&db_tab, stmt, pos++, (uint64_t)vals[3].string, -1);
	db_bind(&db_tab, stmt, pos++, (uint64_t)vals[4].string, -1);

	rc = db_eval_stmt(stmt, db_tab.name);
	assert_int_equal(rc, 0);

	rc = db_finalize(stmt);
	assert_int_equal(rc, 0);

	strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S +0000", &tm);

	rc = pg_check_values(state, ras.db, &stmt, &db_tab,
			     vals, ARRAY_SIZE(vals));
	assert_int_equal(rc, 0);
}

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
	int rc = 0;

	if (priv && priv->stmt) {
		rc = db_finalize(priv->stmt);
		assert_int_equal(rc, 0);
	}

	rc = db_close(0, &ras);
	assert_int_equal(rc, 0);
	return rc;
}

static const struct CMUnitTest tests[] = {
	cmocka_unit_test_setup_teardown(test_db_get_sql_type,
					tests_setup, tests_teardown),
	cmocka_unit_test_setup_teardown(test_db_create_table,
					tests_setup, tests_teardown),
	cmocka_unit_test_setup_teardown(test_db_bind_type,
					tests_setup, tests_teardown),
	cmocka_unit_test_setup_teardown(test_db_bind,
					tests_setup, tests_teardown),
	cmocka_unit_test_setup_teardown(test_db_complex_table,
					tests_setup, tests_teardown),
};

static int group_setup(void **state)
{
	const char *port;

	conn_parms.host = env_or("RAS_PG_HOST", "");
	if (!conn_parms.host || !conn_parms.host[0])
		conn_parms.host = NULL;

	conn_parms.user = env_or("RAS_PG_USER", "rasdaemon");
	conn_parms.password = env_or("RAS_PG_PASSWORD", "");
	conn_parms.database = env_or("RAS_PG_DATABASE", "rasdaemon_test");

	port = getenv("RAS_PG_PORT");
	if (port)
		conn_parms.port = (unsigned short)atoi(port);

	return module_init(&ras, "db-postgresql");
}

static int group_teardown(void **state)
{
	module_cleanup("db-postgresql");
	return 0;
}

int test_postgresql(void)
{
	return _cmocka_run_group_tests("postgresql backend",
					   tests,
					   ARRAY_SIZE(tests),
					   group_setup,
					   group_teardown);
}
