// SPDX-License-Identifier: GPL-2.0-or-later
/*
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

typedef struct db_table_descriptor_list (*table_getter)(void);

static const table_getter table_getters[] = {
	ras_record_table_descriptors,
#ifdef HAVE_AMP_NS_DECODE
	ampere_table_descriptors,
#endif
#ifdef HAVE_HISI_NS_DECODE
	hip08_table_descriptors,
	hisilicon_table_descriptors,
#endif
#ifdef HAVE_JAGUAR_NS_DECODE
	jaguarmicro_table_descriptors,
#endif
#ifdef HAVE_NVIDIA_NS_DECODE
	nvidia_table_descriptors,
#endif
#ifdef HAVE_YITIAN_NS_DECODE
	yitian_table_descriptors,
#endif
};

static uint64_t field_value(enum db_field_type type, unsigned int row,
			    int *length)
{
	static const char text[] = "database unit test";
	static const char timestamp[] = "2026-08-21 12:00:00 +0000";
	static const unsigned char blob[] = { 0x52, 0x41, 0x53, 0xdb };

	*length = -1;
	switch (type) {
	case DB_TYPE_TIMESTAMP:
		return (uint64_t)timestamp;
	case DB_TYPE_TEXT:
		return (uint64_t)text;
	case DB_TYPE_BLOB:
		*length = sizeof(blob);
		return (uint64_t)blob;
	case DB_TYPE_INT32:
	case DB_TYPE_INT64:
		return row + 1;
	case DB_TYPE_SERIAL:
		return 0;
	}

	return 0;
}

static void populate_table(const struct db_table_descriptor *table)
{
	struct ras_stmt *stmt = NULL;
	unsigned int row;
	int rc;

	rc = db_create_table_prep_stmt(&ras, &stmt, table);
	assert_int_equal(rc, 0);
	assert_non_null(stmt);

	for (row = 0; row < 10; row++) {
		int position = 1;
		size_t field;

		for (field = 0; field < table->num_fields; field++) {
			const enum db_field_type type = table->fields[field].type;
			int length;

			if (type == DB_TYPE_SERIAL)
				continue;
			rc = db_bind(table, stmt, position++,
				     field_value(type, row, &length), length);
			assert_int_equal(rc, 0);
		}

		rc = db_eval_stmt(stmt, table->name);
		assert_int_equal(rc, 0);
	}

	rc = db_finalize(stmt);
	assert_int_equal(rc, 0);
}

void test_database_tables(void **state)
{
	size_t getter;

	(void)state;
	for (getter = 0; getter < ARRAY_SIZE(table_getters); getter++) {
		struct db_table_descriptor_list list = table_getters[getter]();
		size_t table;

		for (table = 0; table < list.num_tables; table++) {
			if (!mock_output)
				print_message("checking database table %s\n",
					      list.tables[table]->name);
			populate_table(list.tables[table]);
		}
	}
}
