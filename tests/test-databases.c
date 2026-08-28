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

void test_database_tables(void **state)
{
	size_t getter;

	for (getter = 0; getter < ARRAY_SIZE(table_getters); getter++) {
		struct db_table_descriptor_list list = table_getters[getter]();
		size_t table;

		for (table = 0; table < list.num_tables; table++) {
				log(TERM, LOG_DEBUG,
				    "checking database table %s\n",
				    list.tables[table]->name);
			populate_table(list.tables[table]);
		}
	}
}

extern const struct db_table_descriptor mc_event_tab;
#ifdef HAVE_AER
extern const struct db_table_descriptor aer_event_tab;
#endif
#ifdef HAVE_NON_STANDARD
extern const struct db_table_descriptor non_standard_event_tab;
#endif
#ifdef HAVE_ARM
extern const struct db_table_descriptor arm_event_tab;
#endif
#ifdef HAVE_EXTLOG
extern const struct db_table_descriptor extlog_event_tab;
#endif
#ifdef HAVE_MCE
extern const struct db_table_descriptor mce_record_tab;
#endif
#ifdef HAVE_DEVLINK
extern const struct db_table_descriptor devlink_event_tab;
#endif
#ifdef HAVE_DISKERROR
extern const struct db_table_descriptor diskerror_event_tab;
#endif
#ifdef HAVE_MEMORY_FAILURE
extern const struct db_table_descriptor mf_event_tab;
#endif
#ifdef HAVE_CXL
extern const struct db_table_descriptor cxl_poison_event_tab;
extern const struct db_table_descriptor cxl_aer_ue_event_tab;
extern const struct db_table_descriptor cxl_aer_ce_event_tab;
extern const struct db_table_descriptor cxl_overflow_event_tab;
extern const struct db_table_descriptor cxl_generic_event_tab;
extern const struct db_table_descriptor cxl_general_media_event_tab;
extern const struct db_table_descriptor cxl_dram_event_tab;
extern const struct db_table_descriptor cxl_memory_module_event_tab;
#endif
#ifdef HAVE_SIGNAL
extern const struct db_table_descriptor signal_event_tab;
#endif
#ifdef HAVE_RERI
extern const struct db_table_descriptor reri_event_tab;
#endif

struct db_table_descriptor_list ras_record_table_descriptors(void)
{
	static const struct db_table_descriptor * const tables[] = {
		&mc_event_tab,
#ifdef HAVE_AER
		&aer_event_tab,
#endif
#ifdef HAVE_NON_STANDARD
		&non_standard_event_tab,
#endif
#ifdef HAVE_ARM
		&arm_event_tab,
#endif
#ifdef HAVE_EXTLOG
		&extlog_event_tab,
#endif
#ifdef HAVE_MCE
		&mce_record_tab,
#endif
#ifdef HAVE_DEVLINK
		&devlink_event_tab,
#endif
#ifdef HAVE_DISKERROR
		&diskerror_event_tab,
#endif
#ifdef HAVE_MEMORY_FAILURE
		&mf_event_tab,
#endif
#ifdef HAVE_CXL
		&cxl_poison_event_tab,
		&cxl_aer_ue_event_tab,
		&cxl_aer_ce_event_tab,
		&cxl_overflow_event_tab,
		&cxl_generic_event_tab,
		&cxl_general_media_event_tab,
		&cxl_dram_event_tab,
		&cxl_memory_module_event_tab,
#endif
#ifdef HAVE_SIGNAL
		&signal_event_tab,
#endif
#ifdef HAVE_RERI
		&reri_event_tab,
#endif
	};

	return (struct db_table_descriptor_list) {
		.tables = tables,
		.num_tables = ARRAY_SIZE(tables),
	};
}
