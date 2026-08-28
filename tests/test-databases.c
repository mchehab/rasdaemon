// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2026 Mauro Carvalho Chehab <mchehab+huawei@kernel.org>
 *
 * Exercise every production database table descriptor against each enabled
 * backend.  Backend-specific test suites provide the open connection.
 */

#include "config.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "core/ras-events.h"
#include "core/ras-logger.h"
#include "db/ras-db.h"
#include "tests/unittest.h"

extern struct ras_events ras;

static const char text[] = "database unit test";
static const char timestamp[] = "2026-08-21 12:00:00 +00:00";
static const unsigned char blob[] = { 0x52, 0x41, 0x53, 0xdb };

static void populate_table(const struct db_table_descriptor *table)
{
	struct ras_stmt *stmt = NULL;
	unsigned int row;
	int rc;

	rc = db_create_table(ras.db, table);
	assert_int_equal(rc, 0);
	rc = db_prepare_insert_stmt(ras.db, &stmt, table);
	assert_int_equal(rc, 0);
	assert_non_null(stmt);

	for (row = 0; row < 10; row++) {
		int position = 1;
		size_t field;
		uint64_t val;

		for (field = 0; field < table->num_fields; field++) {
			const enum db_field_type type = table->fields[field].type;
			int length = -1;

			switch (type) {
				case DB_TYPE_SERIAL:
					continue;
				case DB_TYPE_INT32:
				case DB_TYPE_INT64:
					val = row + 1;
					break;
				case DB_TYPE_TIMESTAMP:
					val = (uint64_t)timestamp;
					break;
				default:
				case DB_TYPE_TEXT:
					val = (uint64_t)text;
					break;
				case DB_TYPE_BLOB:
					length = sizeof(blob);
					val = (uint64_t)blob;
					break;
			}

			rc = db_bind(table, stmt, position++, val, length);

			assert_int_equal(rc, 0);
		}

		rc = db_eval_stmt(stmt, table->name);
		assert_int_equal(rc, 0);
	}

	rc = db_finalize(stmt);
	assert_int_equal(rc, 0);
}

static int populate_registered_table(const struct db_table_descriptor *table,
				     void *data)
{
	size_t *count = data;

	assert_non_null(table);
	assert_non_null(table->name);
	assert_non_null(table->fields);
	assert_true(table->num_fields > 1);
	log(TERM, LOG_DEBUG, "checking database table %s\n", table->name);
	populate_table(table);
	(*count)++;

	return 0;
}

void test_database_tables(void **state)
{
	size_t count = 0;

	assert_int_equal(ras_db_table_test_foreach(populate_registered_table,
						   &count), 0);
	assert_true(count > 0);
}
