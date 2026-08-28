// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2026 Mauro Carvalho Chehab <mchehab+huawei@kernel.org>
 */

#include "config.h"

#include <errno.h>
#include <signal.h>
/* for sqlite3 flags */
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "core/modules.h"
#include "core/ras-env.h"
#include "core/ras-events.h"
#include "core/ras-logger.h"
#include "db/db-sqlite3.h"
#include "db/ras-db.h"
#include "events-arch-arm/ras-arm-handler.h"
#include "events-arch-arm/ras-non-standard-handler.h"
#include "events-arch-riscv/ras-reri-handler.h"
#include "events-arch-x86/ras-mce-handler.h"
#include "events/ras-aer-handler.h"
#include "events/ras-cxl-handler.h"
#include "events/ras-devlink-handler.h"
#include "events/ras-diskerror-handler.h"
#include "events/ras-extlog-handler.h"
#include "events/ras-mc-handler.h"
#include "events/ras-memory-failure-handler.h"
#include "events/ras-signal-handler.h"
#include "tests/unittest.h"

#ifdef HAVE_BLK_RQ_ERROR
#define DISKERROR_TRACE_EVENT "block_rq_error"
#else
#define DISKERROR_TRACE_EVENT "block_rq_complete"
#endif

extern struct module_list ras_modules;

extern struct ras_events ras;

struct mock_priv {
	struct ras_stmt *stmt;
};

static struct db_sqlite3_conn_params conn_parms = {
	.database = "/tmp/sqlite3_mock.db",
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

static void sqlite3_assert_index(const char *table, const char *field)
{
	sqlite3 *db = (void *)ras.db;
	sqlite3_stmt *stmt = NULL;
	char sql[256];

	snprintf(sql, sizeof(sql), "PRAGMA index_info('%s_%s_idx')",
		 table, field);
	assert_int_equal(sqlite3_prepare_v2(db, sql, -1, &stmt, NULL),
			 SQLITE_OK);
	assert_int_equal(sqlite3_step(stmt), SQLITE_ROW);
	assert_string_equal((const char *)sqlite3_column_text(stmt, 2), field);
	assert_int_equal(sqlite3_step(stmt), SQLITE_DONE);
	assert_int_equal(sqlite3_finalize(stmt), SQLITE_OK);
}

static void sqlite3_compare_value(void **state,
				  int pos,
				  const struct db_values *expected,
				  sqlite3_stmt *stmt)
{
	int type = sqlite3_column_type(stmt, pos);
	const char *str;
	char msg[256];
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

	if (!expected->string) {
		snprintf(msg, sizeof(msg), "element #%d should be NULL", pos);
		assert_null_msg(str, msg);
		return;
	}

	snprintf(msg, sizeof(msg),
		 "element #%d should be a string instead of NULL", pos);
	assert_non_null_msg(str, msg);
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

static void sqlite3_assert_row_count(sqlite3 *db, const char *table,
				     int expected)
{
	sqlite3_stmt *stmt = NULL;
	char sql[256];

	snprintf(sql, sizeof(sql), "SELECT COUNT(*) FROM %s", table);
	assert_int_equal(sqlite3_prepare_v2(db, sql, -1, &stmt, NULL),
			 SQLITE_OK);
	assert_int_equal(sqlite3_step(stmt), SQLITE_ROW);
	assert_int_equal(sqlite3_column_int(stmt, 0), expected);
	assert_int_equal(sqlite3_finalize(stmt), SQLITE_OK);
}

static void init_cxl_header(struct ras_cxl_event_common_hdr *header)
{
	memset(header, 0, sizeof(*header));
	strscpy(header->timestamp, "2026-08-25 12:00:00 +0000",
		sizeof(header->timestamp));
	header->memdev = "mem0";
	header->host = "host0";
	header->serial = 0x1234;
	header->log_type = "Informational";
	header->hdr_uuid = "00112233-4455-6677-8899-aabbccddeeff";
	strscpy(header->hdr_timestamp, "2026-08-25 12:00:00 +0000",
		sizeof(header->hdr_timestamp));
}

/*
 * Tests
 */

/* Check if type mapping is done the right way */
static void test_db_get_sql_type(void **state)
{
	const char *type_str;

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
		{ .name = "time", .type = DB_TYPE_TIMESTAMP  },
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
		{
			.type = fields[2].type,
			.string = NULL,
		},
	};

	db_create_table(ras.db, &db_tab);

	rc = db_prepare_insert_stmt(ras.db, &stmt, &db_tab);
	assert_int_equal(rc, 0);
	assert_non_null(stmt);

	/* No bindings here */

	rc = db_eval_stmt(stmt, db_tab.name);
	assert_int_equal(rc, 0);

	rc = db_finalize(stmt);
	assert_int_equal(rc, 0);

	rc = sqlite3_check_values(state, ras.db, &stmt, &db_tab,
				  vals, ARRAY_SIZE(vals));
	assert_int_equal(rc, 0);
}

/* Check db_bind with numeric and text fields. */
static void test_db_bind_types(void **state)
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

	rc = db_create_table(ras.db, &db_tab);
	assert_int_equal(rc, 0);
	rc = db_prepare_insert_stmt(ras.db, &stmt, &db_tab);
	assert_int_equal(rc, 0);
	assert_non_null(stmt);

	assert_int_equal(db_bind(&db_tab, stmt, pos++,
				 (uint64_t)vals[0].value, -1), 0);
	assert_int_equal(db_bind(&db_tab, stmt, pos++,
				 (uint64_t)vals[1].string, -1), 0);
	assert_int_equal(db_bind(&db_tab, stmt, pos++,
				 (uint64_t)vals[2].value, -1), 0);

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

	assert_int_equal(db_create_table(ras.db, &db_tab), 0);
	assert_int_equal(db_prepare_insert_stmt(ras.db, &stmt, &db_tab), 0);
	assert_non_null(stmt);

	db_bind(&db_tab, stmt, pos++, (uint64_t)vals[1].string, -1);
	db_bind(&db_tab, stmt, pos++, (uint64_t)vals[2].string, -1);

	rc = db_eval_stmt(stmt, db_tab.name);
	assert_int_equal(rc, 0);
	assert_int_equal(db_finalize(stmt), 0);

	rc = sqlite3_check_values(state, ras.db, &stmt, &db_tab,
				  vals, ARRAY_SIZE(vals));
	assert_int_equal(rc, 0);
}

/* Test table creation */
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

	/* Open a database connection first */

	rc = db_create_table(ras.db, &db_tab);
	assert_int_equal(rc, 0);
	sqlite3_assert_index(db_tab.name, "val");
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
		{ .name = "timestamp", .type = DB_TYPE_TIMESTAMP,
		  .create_index = true },
	};

	static const struct db_table_descriptor db_tab = {
		.name = "test_tbl",
		.fields = fields,
		.num_fields = ARRAY_SIZE(fields),
	};

	rc = db_create_table(ras.db, &org_table_desc);
	assert_int_equal(rc, 0);

	rc = db_alter_table(ras.db, &stmt, &db_tab);
	assert_int_equal(rc, 0);
	sqlite3_assert_index(db_tab.name, "timestamp");
}

static void test_db_complex_table(void **state)
{
	struct mock_priv *priv = ras.db_priv;
	struct ras_stmt *stmt = priv->stmt;
	int rc, pos = 1;

	static const struct db_fields fields[] = {
		{ .name = "timestamp",	.type = DB_TYPE_TIMESTAMP, .create_index = true },
		{ .name = "id",		.type = DB_TYPE_SERIAL, .is_pk = true },
		{ .name = "sec_type",	.type = DB_TYPE_BLOB },
		{ .name = "severity",	.type = DB_TYPE_TEXT },
		{ .name = "error",	.type = DB_TYPE_BLOB },
	};

	static const struct db_table_descriptor db_tab = {
		.name = "test_tbl",
		.fields = fields,
		.num_fields = sizeof(fields) / sizeof(fields[0]),
	};
	static struct db_values vals[] = {
		{
			.type = fields[0].type,
			.string = "2026-01-01 12:59:30",
		},
		{
			.type = fields[1].type,
			.value = 1,
		},
		{
			.type = fields[2].type,
			.string = "blob1",
		},
		{
			.type = fields[3].type,
			.string = "text",
		},
		{
			.type = fields[4].type,
			.string = "blob2",
		},
	};

	rc = db_create_table(ras.db, &db_tab);
	assert_int_equal(rc, 0);
	rc = db_prepare_insert_stmt(ras.db, &stmt, &db_tab);
	assert_int_equal(rc, 0);
	assert_non_null(stmt);

	db_bind(&db_tab, stmt, pos++, (uint64_t)vals[0].string, -1);
	db_bind(&db_tab, stmt, pos++, (uint64_t)vals[2].string, -1);
	db_bind(&db_tab, stmt, pos++, (uint64_t)vals[3].string, -1);
	db_bind(&db_tab, stmt, pos++, (uint64_t)vals[4].string, -1);

	rc = db_eval_stmt(stmt, db_tab.name);
	assert_int_equal(rc, 0);
	assert_int_equal(db_finalize(stmt), 0);

	rc = sqlite3_check_values(state, ras.db, &stmt, &db_tab,
				  vals, ARRAY_SIZE(vals));
	assert_int_equal(rc, 0);
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

static void test_database_environment(void **state)
{
	char config_path[128], path[128];
	const char *current;
	char *saved = NULL;
	FILE *fp;
	int rc;

	current = getenv("RAS_SQLITE3_DATABASE");
	if (current)
		saved = strdup(current);

	snprintf(path, sizeof(path), "/tmp/rasdaemon-sqlite-env-%ld.db",
		 (long)getpid());
	snprintf(config_path, sizeof(config_path),
		 "/tmp/rasdaemon-sqlite-env-%ld.conf", (long)getpid());
	unlink(path);
	unsetenv("RAS_SQLITE3_DATABASE");
	fp = fopen(config_path, "w");
	assert_non_null(fp);
	assert_true(fprintf(fp, "RAS_SQLITE3_DATABASE=\"%s\"\n", path) > 0);
	assert_int_equal(fclose(fp), 0);
	assert_int_equal(ras_set_env(config_path), 0);
	unlink(config_path);

	rc = db_open(NULL, 0, &ras, sizeof(struct mock_priv));
	assert_int_equal(rc, 0);
	assert_non_null(ras.db);
	assert_int_equal(db_close(0, &ras), 0);
	assert_int_equal(access(path, F_OK), 0);

	unlink(path);
	if (saved) {
		assert_int_equal(setenv("RAS_SQLITE3_DATABASE", saved, 1), 0);
		free(saved);
	} else {
		assert_int_equal(unsetenv("RAS_SQLITE3_DATABASE"), 0);
	}
}

static void test_mc_event_recording(void **state)
{
	const char *database = "/tmp/rasdaemon-mc-event-test.db";
	const char *current;
	char *saved = NULL;
	struct ras_mc_event mc = {
		.timestamp = "2026-08-25 12:00:00 +0000",
		.error_count = 1,
		.error_type = "Corrected",
		.msg = "unit test",
		.label = "DIMM0",
		.mc_index = 0,
		.top_layer = 0,
		.middle_layer = 1,
		.lower_layer = 2,
		.address = 0x1234,
		.grain = 64,
		.syndrome = 0,
		.driver_detail = "test",
	};
	sqlite3 *db;
	int rc;
#ifdef HAVE_AER
	struct ras_aer_event aer = { .error_type = "Corrected",
		.dev_name = "0000:01:00.0", .msg = "Receiver Error" };
#endif
#ifdef HAVE_MCE
	struct mce_event mce = { .status = MCI_STATUS_VAL, .cpu = 1 };
#endif
#ifdef HAVE_EXTLOG
	static const unsigned char fru_id[16] = { 1 };
	static const unsigned char cper_data[8] = { 2 };
	struct ras_extlog_event extlog = { .etype = 2, .error_seq = 1,
		.severity = 2, .address = 0x1000,
		.fru_id = (const char *)fru_id, .fru_text = "DIMM0",
		.cper_data = (const char *)cper_data,
		.cper_data_length = sizeof(cper_data) };
#endif
#ifdef HAVE_NON_STANDARD
	static const unsigned char section[16] = { 3 };
	static const unsigned char ns_fru[16] = { 4 };
	static const unsigned char ns_data[8] = { 5 };
	struct ras_non_standard_event non_standard = {
		.sec_type = (const char *)section,
		.fru_id = (const char *)ns_fru, .fru_text = "board",
		.severity = "Corrected", .error = ns_data,
		.length = sizeof(ns_data),
	};
#endif
#ifdef HAVE_ARM
	static const unsigned char arm_data[8] = { 6 };
	struct ras_arm_event arm = { .error_count = 1, .affinity = 2,
		.pei_error = arm_data, .pei_len = sizeof(arm_data),
		.ctx_error = arm_data, .ctx_len = sizeof(arm_data),
		.vsei_error = arm_data, .oem_len = sizeof(arm_data),
		.error_info = 7 };
#endif
#ifdef HAVE_DEVLINK
	struct devlink_event devlink = { .bus_name = "pci",
		.dev_name = "0000:01:00.0", .driver_name = "driver",
		.reporter_name = "fw", .msg = "health error" };
#endif
#ifdef HAVE_DISKERROR
	struct diskerror_event disk = { .dev = "8:1", .sector = 16,
		.nr_sector = 8, .error = "I/O error", .rwbs = "R",
		.cmd = "read" };
#endif
#ifdef HAVE_MEMORY_FAILURE
	struct ras_mf_event memory_failure = { .pfn = "0x123",
		.page_type = "huge page", .action_result = "Recovered" };
#endif
#ifdef HAVE_CXL
	uint32_t header_log[CXL_HEADERLOG_SIZE_U32] = { 0 };
	uint8_t event_data[CXL_EVENT_RECORD_DATA_LENGTH] = { 0 };
	uint8_t component_id[CXL_EVENT_GEN_MED_COMP_ID_SIZE] = { 0 };
	uint8_t correction_mask[CXL_EVENT_DER_CORRECTION_MASK_SIZE] = { 0 };
	struct ras_cxl_poison_event poison = { .memdev = "mem0",
		.host = "host0", .serial = 1, .trace_type = "List",
		.region = "region0", .uuid = "uuid", .source = "Internal" };
	struct ras_cxl_aer_ue_event cxl_ue = { .memdev = "mem0",
		.host = "host0", .serial = 1, .error_status = 1,
		.header_log = header_log };
	struct ras_cxl_aer_ce_event cxl_ce = { .memdev = "mem0",
		.host = "host0", .serial = 1, .error_status = 1 };
	struct ras_cxl_overflow_event overflow = { .memdev = "mem0",
		.host = "host0", .serial = 1, .log_type = "Warning",
		.count = 2 };
	struct ras_cxl_generic_event generic = { .data = event_data };
	struct ras_cxl_general_media_event media = { .comp_id = component_id,
		.region = "region0", .region_uuid = "uuid" };
	struct ras_cxl_dram_event dram = { .cor_mask = correction_mask,
		.comp_id = component_id, .region = "region0",
		.region_uuid = "uuid" };
	struct ras_cxl_memory_module_event module = { .comp_id = component_id };
#endif
#ifdef HAVE_SIGNAL
	struct ras_signal_event signal = { .sig = SIGBUS, .code = BUS_ADRERR,
		.comm = "worker", .pid = 42, .group = 1 };
#endif
#ifdef HAVE_RERI
	struct ras_reri_event reri = { .err_src_id = 1,
		.source_type = RERI_SOURCE_TYPE_IOMMU,
		.severity = RERI_SEV_CORRECTED, .status = 1 };
#endif

	current = getenv("RAS_SQLITE3_DATABASE");
	if (current)
		saved = strdup(current);

	assert_int_equal(setenv("RAS_SQLITE3_DATABASE", database, 1), 0);
	unlink(database);

	rc = db_open(NULL, 0, &ras, 0);
	assert_int_equal(rc, 0);
	assert_non_null(ras.db);

	ras.record_events = true;
	rc = ras_event_publish(&ras, MC_EVENT, &mc);
	assert_int_equal(rc, 0);

	db = (sqlite3 *)ras.db;
	sqlite3_assert_row_count(db, "mc_event", 1);

#define RECORD_AND_CHECK(call, table) do { \
	assert_int_equal((call), 0); \
	sqlite3_assert_row_count(db, table, 1); \
} while (0)

#ifdef HAVE_AER
	strscpy(aer.timestamp, mc.timestamp, sizeof(aer.timestamp));
	RECORD_AND_CHECK(ras_event_publish(&ras, AER_EVENT, &aer),
			 "aer_event");
#endif
#ifdef HAVE_MCE
	strscpy(mce.timestamp, mc.timestamp, sizeof(mce.timestamp));
	RECORD_AND_CHECK(ras_event_publish(&ras, MCE_EVENT, &mce), "mce_record");
#endif
#ifdef HAVE_EXTLOG
	strscpy(extlog.timestamp, mc.timestamp, sizeof(extlog.timestamp));
	RECORD_AND_CHECK(ras_event_publish(&ras, EXTLOG_EVENT, &extlog),
			 "extlog_event");
#endif
#ifdef HAVE_NON_STANDARD
	strscpy(non_standard.timestamp, mc.timestamp,
		sizeof(non_standard.timestamp));
	RECORD_AND_CHECK(ras_event_publish(&ras, NON_STANDARD_EVENT,
					   &non_standard),
			 "non_standard_event");
#endif
#ifdef HAVE_ARM
	strscpy(arm.timestamp, mc.timestamp, sizeof(arm.timestamp));
	RECORD_AND_CHECK(ras_event_publish(&ras, ARM_EVENT, &arm),
			 "arm_event");
#endif
#ifdef HAVE_DEVLINK
	strscpy(devlink.timestamp, mc.timestamp, sizeof(devlink.timestamp));
	RECORD_AND_CHECK(ras_event_publish(&ras, DEVLINK_EVENT, &devlink),
			 "devlink_event");
#endif
#ifdef HAVE_DISKERROR
	strscpy(disk.timestamp, mc.timestamp, sizeof(disk.timestamp));
	RECORD_AND_CHECK(ras_event_publish(&ras, DISKERROR_EVENT, &disk),
			 "disk_errors");
#endif
#ifdef HAVE_MEMORY_FAILURE
	strscpy(memory_failure.timestamp, mc.timestamp,
		sizeof(memory_failure.timestamp));
	RECORD_AND_CHECK(ras_event_publish(&ras, MF_EVENT, &memory_failure),
			 "memory_failure_event");
#endif
#ifdef HAVE_CXL
	strscpy(poison.timestamp, mc.timestamp, sizeof(poison.timestamp));
	strscpy(poison.overflow_ts, mc.timestamp, sizeof(poison.overflow_ts));
	RECORD_AND_CHECK(ras_event_publish(&ras, CXL_POISON_EVENT, &poison),
			 "cxl_poison_event");
	strscpy(cxl_ue.timestamp, mc.timestamp, sizeof(cxl_ue.timestamp));
	RECORD_AND_CHECK(ras_event_publish(&ras, CXL_AER_UE_EVENT, &cxl_ue),
			 "cxl_aer_ue_event");
	strscpy(cxl_ce.timestamp, mc.timestamp, sizeof(cxl_ce.timestamp));
	RECORD_AND_CHECK(ras_event_publish(&ras, CXL_AER_CE_EVENT, &cxl_ce),
			 "cxl_aer_ce_event");
	strscpy(overflow.timestamp, mc.timestamp, sizeof(overflow.timestamp));
	strscpy(overflow.first_ts, mc.timestamp, sizeof(overflow.first_ts));
	strscpy(overflow.last_ts, mc.timestamp, sizeof(overflow.last_ts));
	RECORD_AND_CHECK(ras_event_publish(&ras, CXL_OVERFLOW_EVENT, &overflow),
			 "cxl_overflow_event");
	init_cxl_header(&generic.hdr);
	RECORD_AND_CHECK(ras_event_publish(&ras, CXL_GENERIC_EVENT, &generic),
			 "cxl_generic_event");
	init_cxl_header(&media.hdr);
	RECORD_AND_CHECK(ras_event_publish(&ras, CXL_GENERAL_MEDIA_EVENT, &media),
			 "cxl_general_media_event");
	init_cxl_header(&dram.hdr);
	RECORD_AND_CHECK(ras_event_publish(&ras, CXL_DRAM_EVENT, &dram),
			 "cxl_dram_event");
	init_cxl_header(&module.hdr);
	RECORD_AND_CHECK(ras_event_publish(&ras, CXL_MEMORY_MODULE_EVENT, &module),
			 "cxl_memory_module_event");
#endif
#ifdef HAVE_SIGNAL
	strscpy(signal.timestamp, mc.timestamp, sizeof(signal.timestamp));
	RECORD_AND_CHECK(ras_event_publish(&ras, SIGNAL_EVENT, &signal),
			 "signal_event");
#endif
#ifdef HAVE_RERI
	strscpy(reri.timestamp, mc.timestamp, sizeof(reri.timestamp));
	RECORD_AND_CHECK(ras_event_publish(&ras, RERI_EVENT, &reri),
			 "reri_event");
#endif

#undef RECORD_AND_CHECK

	test_ras_mc_ctl_types("sqlite3", &ras);
	rc = db_close(0, &ras);
	assert_int_equal(rc, 0);
	ras.record_events = false;
	test_ras_mc_ctl_count("sqlite3", "mc_event", 1);

	unlink(database);
	if (saved) {
		assert_int_equal(setenv("RAS_SQLITE3_DATABASE", saved, 1), 0);
		free(saved);
	} else {
		assert_int_equal(unsetenv("RAS_SQLITE3_DATABASE"), 0);
	}
}

static void test_db_open_registered_tables(void **state)
{
	static const struct db_fields fields[] = {
		{ .name = "id", .type = DB_TYPE_SERIAL, .is_pk = true },
		{ .name = "value", .type = DB_TYPE_INT32 },
	};
	static const struct db_table_descriptor desc[] = {
		{ .name = "registered_one", .fields = fields, .num_fields = 2 },
		{ .name = "registered_two", .fields = fields, .num_fields = 2 },
		{ .name = "registered_three", .fields = fields, .num_fields = 2 },
	};
	struct db_desc_and_stmt tables[] = {
		{ .desc = &desc[0] }, { .desc = &desc[1] }, { .desc = &desc[2] },
	};
	struct ras_events test_ras = { 0 };
	struct ras_module_ctx ctx = { 0 };
	struct db_values values[2];
	int rc;

	for (size_t i = 0; i < ARRAY_SIZE(tables); i++)
		assert_int_equal(ras_db_table_register(&ctx, &tables[i]), 0);
	assert_int_equal(db_open(&backend, 0, &test_ras, sizeof(struct mock_priv)), 0);
	for (size_t i = 0; i < ARRAY_SIZE(tables); i++) {
		values[0] = (struct db_values) { .type = DB_TYPE_SERIAL, .value = 1 };
		values[1] = (struct db_values) { .type = DB_TYPE_INT32, .value = i + 1 };
		assert_int_equal(db_bind(&desc[i], tables[i].stmt, 1, values[1].value, -1), 0);
		assert_int_equal(db_eval_stmt(tables[i].stmt, desc[i].name), 0);
		rc = sqlite3_check_values(state, test_ras.db, &tables[i].stmt,
					  &desc[i], values, ARRAY_SIZE(values));
		assert_int_equal(rc, 0);
		tables[i].stmt = NULL;
	}
	assert_int_equal(db_close(0, &test_ras), 0);
	ras_db_table_unregister(&ctx);
}

static void test_db_reference_count(void **state)
{
	char database[128];
	const char *current = getenv("RAS_SQLITE3_DATABASE");
	char *saved = current ? strdup(current) : NULL;
	struct ras_events test_ras = { 0 };

	snprintf(database, sizeof(database),
		 "/tmp/rasdaemon-session-test-%ld.db", (long)getpid());
	assert_int_equal(setenv("RAS_SQLITE3_DATABASE", database, 1), 0);
	unlink(database);

	assert_int_equal(db_open(NULL, 0, &test_ras, 0), 0);
	assert_int_equal(test_ras.db_ref_count, 1);
	assert_non_null(test_ras.db);
	assert_null(test_ras.db_priv);
	assert_int_equal(db_open(NULL, 1, &test_ras, 0), 0);
	assert_int_equal(test_ras.db_ref_count, 2);
	assert_int_equal(db_close(0, &test_ras), 0);
	assert_int_equal(test_ras.db_ref_count, 1);
	assert_non_null(test_ras.db);
	assert_int_equal(db_close(1, &test_ras), 0);
	assert_int_equal(test_ras.db_ref_count, 0);
	assert_null(test_ras.db);
	assert_null(test_ras.db_priv);

	unlink(database);
	if (saved) {
		assert_int_equal(setenv("RAS_SQLITE3_DATABASE", saved, 1), 0);
		free(saved);
	} else {
		assert_int_equal(unsetenv("RAS_SQLITE3_DATABASE"), 0);
	}
}

static const struct CMUnitTest tests[] = {
	cmocka_unit_test(test_db_open_registered_tables),
	cmocka_unit_test(test_db_reference_count),
	cmocka_unit_test(test_database_environment),
	cmocka_unit_test(test_mc_event_recording),

	cmocka_unit_test_setup_teardown(test_database_tables,
					tests_setup, tests_teardown),

	cmocka_unit_test_setup_teardown(test_db_get_sql_type,
					tests_setup, tests_teardown),

	cmocka_unit_test_setup_teardown(test_db_create_table,
					tests_setup, tests_teardown),

	cmocka_unit_test_setup_teardown(test_db_no_binding,
					tests_setup, tests_teardown),

	cmocka_unit_test_setup_teardown(test_db_bind_types,
					tests_setup, tests_teardown),
	cmocka_unit_test_setup_teardown(test_db_bind,
					tests_setup, tests_teardown),

	cmocka_unit_test_setup_teardown(test_db_alter_table,
					tests_setup, tests_teardown),

	cmocka_unit_test_setup_teardown(test_db_complex_table,
					tests_setup, tests_teardown),
};

static int group_setup(void **state)
{
	return db_backend_enable("sqlite3");
}

static int group_teardown(void **state)
{
	module_cleanup("db-sqlite3");
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

REGISTER_TEST(TEST_GROUP_DB_SQLITE3, test_sqlite3, 0);
