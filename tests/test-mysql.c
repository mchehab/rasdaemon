// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2026 Mauro Carvalho Chehab <mchehab+huawei@kernel.org>
 *
 * Unit tests for the MySQL / MariaDB backend.
 *
 * Optional environment variables:
 *   RAS_MYSQL_HOST       (default: ""  = local socket)
 *   RAS_MYSQL_PORT       (default: 3306)
 *   RAS_MYSQL_USER       (default: "rasdaemon")
 *   RAS_MYSQL_PASSWORD   (default: "")
 *   RAS_MYSQL_DATABASE   (default: "rasdaemon_test")
 *   RAS_MYSQL_SOCKET     (default: client default)
 *   RAS_MYSQL_USE_SSL    (default: false)
 *   RAS_MYSQL_CONNECT_TIMEOUT (default: 10 seconds)
 *
 */

#include "config.h"

#include <errno.h>
#include <mysql/mysql.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "core/modules.h"
#include "core/ras-events.h"
#include "core/ras-logger.h"
#include "db/db-mysql.h"
#include "db/ras-db.h"
#include "db/ras-record.h"
#include "tests/unittest.h"

extern struct module_list ras_modules;

extern struct ras_events ras;

struct mock_priv {
	struct ras_stmt *stmt;
};

static struct db_mysql_conn_params conn_parms = {
	.host	  = NULL,
	.port	  = 3306,
	.user	  = "rasdaemon",
	.password  = "mypass",
	.database  = "rasdaemon_test",
	.socket	= NULL,
	.use_ssl   = false,
	.connect_timeout = 10,
};

static struct db_backend backend = {
	.name = "mysql",
	.conn_parms = &conn_parms,
};

struct db_values {
	enum db_field_type	type;
	uint64_t		value;
	char			*string;
};

static char *datetime_to_iso(const char * cell)
{
	struct tm tm = {0};
	char buffer[64];
	char *end;
	char *output;
	size_t length;

	end = strptime(cell, "%Y-%m-%d %H:%M:%S", &tm);
	if (!end )
		return NULL;

	/*
	 * MySQL DATETIME has no timezone.
	 */
	tm.tm_isdst = -1;

	if (mktime(&tm) == (time_t)-1)
		return NULL;

	length = strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S %z", &tm);
	if (length == 0)
		return NULL;

	output = malloc(length + 1);
	if (output == NULL)
		return NULL;

	memcpy(output, buffer, length + 1);
	return output;
}

static int mysql_check_values(void **state,
				  struct ras_db *__db,
				  struct ras_stmt **__stmt,
				  const struct db_table_descriptor *db_tab,
				  const struct db_values *values, int len)
{
	MYSQL *db = (void *)__db;
	char query[1024];
	MYSQL_RES *res;
	MYSQL_ROW row;
	int i, pos, ncol, nrow = 0;

	snprintf(query, sizeof(query), "SELECT * FROM %s", db_tab->name);

	if (mysql_query(db, query)) {
		fprintf(stderr, "mysql_check_values: query failed: %s\n",
			mysql_error(db));
		return -1;
	}

	res = mysql_store_result(db);
	if (!res) {
		fprintf(stderr, "mysql_check_values: no result\n");
		return -1;
	}

	ncol = (int)mysql_field_count(db);

	while ((row = mysql_fetch_row(res))) {
		nrow++;
		pos = 0;
		for (i = 0; i < len && i < ncol; i++) {
			const char *cell = row[i];
			char *converted = NULL;
			const struct db_values *ev = &values[pos++];
			long long val;

			switch (ev->type) {
			case DB_TYPE_SERIAL:
			case DB_TYPE_INT32:
			case DB_TYPE_INT64:
				val = strtoll(cell, NULL, 10);

				assert_int_equal(val, (long long)ev->value);
				break;
			case DB_TYPE_TIMESTAMP:
				converted = datetime_to_iso(cell);
				cell = converted;
				/* fall though */
			case DB_TYPE_TEXT:
			case DB_TYPE_BLOB:
			default:
				if (!ev->string) {
					assert_null_msg(cell, "expected NULL");
				} else {
					assert_non_null_msg(cell, "expected non-NULL");
					assert_string_equal(cell, ev->string);
				}
				break;
			}
			free(converted);

			/* Check hostname */
			if (!i) {
				cell = row[++i];
				assert_string_equal(cell, rasdaemon_hostname);
			}
		}
		assert_int_equal(i, len);
	}

	mysql_free_result(res);
	assert_int_equal(nrow, 1);
	return 0;
}

static void mysql_assert_index(const char *table, const char *field)
{
	MYSQL *db = (void *)ras.db;
	MYSQL_RES *res;
	char query[1024];

	snprintf(query, sizeof(query),
		 "SELECT 1 FROM INFORMATION_SCHEMA.STATISTICS "
		 "WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = '%s' "
		 "AND COLUMN_NAME = '%s'", table, field);
	assert_int_equal(mysql_query(db, query), 0);
	res = mysql_store_result(db);
	assert_non_null(res);
	assert_true(mysql_num_rows(res) > 0);
	mysql_free_result(res);
}

static void test_db_get_sql_type(void **state)
{
	const char *type_str;

	type_str = db_get_sql_type(DB_TYPE_SERIAL, false);
	assert_string_equal(type_str, "BIGINT");

	type_str = db_get_sql_type(DB_TYPE_INT32, false);
	assert_string_equal(type_str, "INT");

	type_str = db_get_sql_type(DB_TYPE_INT64, false);
	assert_string_equal(type_str, "BIGINT");

	type_str = db_get_sql_type(DB_TYPE_TEXT, false);
	assert_string_equal(type_str, "TEXT");

	type_str = db_get_sql_type(DB_TYPE_TIMESTAMP, false);
	assert_string_equal(type_str, "DATETIME(6)");

	type_str = db_get_sql_type(DB_TYPE_BLOB, false);
	assert_string_equal(type_str, "BLOB");

	type_str = db_get_sql_type(DB_TYPE_SERIAL, true);
	assert_string_equal(type_str, "BIGINT AUTO_INCREMENT");
}

static void test_db_create_table(void **state)
{
	int rc;

	static const struct db_fields fields[] = {
		{ .name = "id",   .type = DB_TYPE_SERIAL, .is_pk = true },
		{ .name = "val",  .type = DB_TYPE_INT32,  .create_index = true },
	};

	static const struct db_table_descriptor db_tab = {
		.name = "test_tbl",
		.fields = fields,
		.num_fields = sizeof(fields) / sizeof(fields[0]),
	};

	rc = db_create_table(ras.db, &db_tab);
	assert_int_equal(rc, 0);
	mysql_assert_index(db_tab.name, "hostname");
	mysql_assert_index(db_tab.name, "val");
}

static void test_db_alter_table(void **state)
{
	struct ras_stmt *stmt = NULL;
	int rc;
	static const struct db_fields old_fields[] = {
		{ .name = "id", .type = DB_TYPE_SERIAL, .is_pk = true },
	};
	static const struct db_fields new_fields[] = {
		{ .name = "id", .type = DB_TYPE_SERIAL, .is_pk = true },
		{ .name = "timestamp", .type = DB_TYPE_TIMESTAMP,
		  .create_index = true },
	};
	static const struct db_table_descriptor old_tab = {
		.name = "test_tbl", .fields = old_fields,
		.num_fields = ARRAY_SIZE(old_fields),
	};
	static const struct db_table_descriptor new_tab = {
		.name = "test_tbl", .fields = new_fields,
		.num_fields = ARRAY_SIZE(new_fields),
	};

	rc = db_create_table(ras.db, &old_tab);
	assert_int_equal(rc, 0);
	rc = db_alter_table(ras.db, &stmt, &new_tab);
	assert_int_equal(rc, 0);
	mysql_assert_index(new_tab.name, "timestamp");
}

static void test_db_bind_type(void **state)
{
	struct ras_stmt *stmt = NULL;
	int rc, pos = 1;

	static const struct db_fields fields[] = {
		{ .name = "id",	.type = DB_TYPE_INT32,	.is_pk = true },
		{ .name = "str",   .type = DB_TYPE_TEXT	  },
		{ .name = "int64", .type = DB_TYPE_INT64  },
	};

	static const struct db_table_descriptor db_tab = {
		.name = "test_tbl",
		.fields = fields,
		.num_fields = sizeof(fields) / sizeof(fields[0]),
	};

	static struct db_values vals[] = {
		{ .type = fields[0].type, .value = 1   },
		{ .type = fields[1].type, .string = "boo" },
		{ .type = fields[2].type, .value = -1L },
	};

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

	rc = mysql_check_values(state, ras.db, &stmt, &db_tab,
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
		{ .type = fields[1].type, .string = "The quick brown fox"     },
		{ .type = fields[2].type, .string = "jumps over the lazy dog" },
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

	rc = mysql_check_values(state, ras.db, &stmt, &db_tab,
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
		{ .name = "timestamp",	.type = DB_TYPE_TIMESTAMP, .create_index = true },
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

	rc = mysql_check_values(state, ras.db, &stmt, &db_tab,
				vals, ARRAY_SIZE(vals));
	assert_int_equal(rc, 0);
}

static void test_ras_mc_ctl_record(void **state)
{
	struct ras_mc_event event = {
		.timestamp = "2026-08-27 09:01:50 +0000",
		.error_count = 1,
		.error_type = "Corrected",
		.msg = "database CLI test",
		.label = "DIMM0",
	};
	int rc;

	rc = ras_mc_event_opendb(0, &ras);
	assert_int_equal(rc, 0);
	rc = db_exec_sql(ras.db, "DELETE FROM mc_event");
	assert_int_equal(rc, 0);
	rc = db_mc_event(&ras, &event);
	assert_int_equal(rc, 0);
	test_ras_mc_ctl_types("mysql", &ras);
	rc = ras_mc_event_closedb(0, &ras);
	assert_int_equal(rc, 0);
	test_ras_mc_ctl_count("mysql", "mc_event", 1);
}

static int test_ras_mc_ctl_teardown(void **state)
{

	/*
	 * Assertions in test_ras_mc_ctl_record() can abort the test before
	 * ras_mc_event_closedb() is reached.  Always release the record
	 * statements here so the following test cannot reuse dangling state.
	 */
	if (ras.db_ref_count > 0)
		return ras_mc_event_closedb(0, &ras);

	return 0;
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
	int rc;

	if (priv && priv->stmt) {
		rc = db_finalize(priv->stmt);
		assert_int_equal(rc, 0);
	}

	rc = db_close(0, &ras);
	assert_int_equal(rc, 0);
	return rc;
}

static const struct CMUnitTest tests[] = {
	cmocka_unit_test_teardown(test_ras_mc_ctl_record,
				  test_ras_mc_ctl_teardown),
	cmocka_unit_test_setup_teardown(test_database_tables,
					tests_setup, tests_teardown),

	cmocka_unit_test_setup_teardown(test_db_get_sql_type,
					tests_setup, tests_teardown),
	cmocka_unit_test_setup_teardown(test_db_create_table,
					tests_setup, tests_teardown),
	cmocka_unit_test_setup_teardown(test_db_alter_table,
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

	/* read connection params from environment */
	conn_parms.host = env_or("RAS_MYSQL_HOST", conn_parms.host);
	if (!conn_parms.host || !conn_parms.host[0])
		conn_parms.host = NULL;

	conn_parms.user = env_or("RAS_MYSQL_USER", conn_parms.user);
	conn_parms.password = env_or("RAS_MYSQL_PASSWORD", conn_parms.password);
	conn_parms.database = env_or("RAS_MYSQL_DATABASE", conn_parms.database);
	conn_parms.socket = env_or("RAS_MYSQL_SOCKET", conn_parms.socket);
	conn_parms.use_ssl = env_or_bool("RAS_MYSQL_USE_SSL", conn_parms.use_ssl);
	conn_parms.connect_timeout = env_or_int("RAS_MYSQL_CONNECT_TIMEOUT",
						  conn_parms.connect_timeout);

	port = getenv("RAS_MYSQL_PORT");
	if (port)
		conn_parms.port = atoi(port);

	if (setenv("RAS_MYSQL_USER", conn_parms.user, 1) ||
	    setenv("RAS_MYSQL_PASSWORD", conn_parms.password, 1) ||
	    setenv("RAS_MYSQL_DATABASE", conn_parms.database, 1) ||
	    setenv("RAS_MYSQL_PORT", port ? port : "3306", 1))
		return -1;
	if (conn_parms.host && setenv("RAS_MYSQL_HOST", conn_parms.host, 1))
		return -1;
	if (conn_parms.socket && setenv("RAS_MYSQL_SOCKET", conn_parms.socket, 1))
		return -1;

	return db_backend_enable("mysql");
}

static int group_teardown(void **state)
{
	module_cleanup("db-mysql");
	return 0;
}

int test_mysql(void)
{
	return _cmocka_run_group_tests("mysql backend",
					   tests,
					   ARRAY_SIZE(tests),
					   group_setup,
					   group_teardown);
}
