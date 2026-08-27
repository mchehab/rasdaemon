// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2026 Mauro Carvalho Chehab <mchehab+huawei@kernel.org>
 */

#include "config.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "tests/unittest.h"
#include "core/ras-events.h"
#include "db/ras-db.h"

#ifndef RAS_SOURCE_DIR
#define RAS_SOURCE_DIR "."
#endif
#ifndef RAS_BUILD_DIR
#define RAS_BUILD_DIR "."
#endif

void test_ras_mc_ctl_count(const char *backend, const char *table,
			   int expected)
{
	char output[128];
	char command[2048];
	FILE *fp;
	char line[256];
	char expected_line[64];
	int rc;

	snprintf(output, sizeof(output), "/tmp/rasdaemon-mc-ctl-%ld.out",
		 (long)getpid());
	snprintf(command, sizeof(command),
		 "RASDAEMON_DB_BACKEND='%s' PYTHONPATH='%s/util:%s' "
		 "python3 '%s/util/ras-mc-ctl.py' --config /dev/null "
		 "database --count --table '%s' > '%s' 2>&1",
		 backend, RAS_SOURCE_DIR, RAS_BUILD_DIR, RAS_SOURCE_DIR, table,
		 output);

	rc = system(command);
	assert_int_equal(rc, 0);
	snprintf(expected_line, sizeof(expected_line), "Count: %d\n", expected);

	fp = fopen(output, "r");
	assert_non_null(fp);
	assert_true(fgets(line, sizeof(line), fp) != NULL);
	assert_string_equal(line, expected_line);
	assert_int_equal(fclose(fp), 0);
	assert_int_equal(unlink(output), 0);
}

void test_ras_mc_ctl_types(const char *backend, struct ras_events *ras)
{
	static const uint8_t blob[] = "blob-value";
	static const struct db_fields fields[] = {
		{ .name = "timestamp", .type = DB_TYPE_TIMESTAMP },
		{ .name = "id", .type = DB_TYPE_SERIAL, .is_pk = true },
		{ .name = "blob_value", .type = DB_TYPE_BLOB },
		{ .name = "text_value", .type = DB_TYPE_TEXT },
		{ .name = "int32_value", .type = DB_TYPE_INT32 },
		{ .name = "int64_value", .type = DB_TYPE_INT64 },
	};
	static const struct db_table_descriptor table = {
		.name = "ras_mc_ctl_types",
		.fields = fields,
		.num_fields = ARRAY_SIZE(fields),
	};
	struct ras_stmt *stmt = NULL;
	int rc, pos = 1;
	char output[128], command[2048], line[512];
	FILE *fp;

	rc = db_exec_sql(ras->db, "DROP TABLE IF EXISTS ras_mc_ctl_types");
	assert_int_equal(rc, 0);
	rc = db_create_table_prep_stmt(ras, &stmt, &table);
	assert_int_equal(rc, 0);
	assert_non_null(stmt);
	db_bind(&table, stmt, pos++, (uint64_t)"2026-08-27 09:01:50 +0000", -1);
	db_bind(&table, stmt, pos++, (uint64_t)blob, sizeof(blob) - 1);
	db_bind(&table, stmt, pos++, (uint64_t)"text-value", -1);
	db_bind(&table, stmt, pos++, 32, -1);
	db_bind(&table, stmt, pos++, 64, -1);
	rc = db_eval_stmt(stmt, table.name);
	assert_int_equal(rc, 0);
	rc = db_finalize(stmt);
	assert_int_equal(rc, 0);

	snprintf(output, sizeof(output), "/tmp/rasdaemon-mc-ctl-types-%ld.out",
		 (long)getpid());
	snprintf(command, sizeof(command),
		 "RASDAEMON_DB_BACKEND='%s' PYTHONPATH='%s/util:%s' "
		 "python3 '%s/util/ras-mc-ctl.py' --config /dev/null "
		 "database --describe --table ras_mc_ctl_types > '%s' 2>&1",
		 backend, RAS_SOURCE_DIR, RAS_BUILD_DIR, RAS_SOURCE_DIR, output);
	rc = system(command);
	assert_int_equal(rc, 0);
	fp = fopen(output, "r");
	assert_non_null(fp);
	line[0] = '\0';
	while (fgets(command, sizeof(command), fp))
		strncat(line, command, sizeof(line) - strlen(line) - 1);
	assert_non_null(strstr(line, "ras_mc_ctl_types:"));
	assert_non_null(strstr(line, "timestamp"));
	assert_non_null(strstr(line, "id"));
	assert_non_null(strstr(line, "blob_value"));
	assert_non_null(strstr(line, "text_value"));
	assert_non_null(strstr(line, "int32_value"));
	assert_non_null(strstr(line, "int64_value"));
	assert_int_equal(fclose(fp), 0);
	assert_int_equal(unlink(output), 0);

	snprintf(command, sizeof(command),
		 "RASDAEMON_DB_BACKEND='%s' PYTHONPATH='%s/util:%s' "
		 "python3 '%s/util/ras-mc-ctl.py' --config /dev/null "
		 "database --errors --table ras_mc_ctl_types > '%s' 2>&1",
		 backend, RAS_SOURCE_DIR, RAS_BUILD_DIR, RAS_SOURCE_DIR, output);
	rc = system(command);
	assert_int_equal(rc, 0);
	fp = fopen(output, "r");
	assert_non_null(fp);
	line[0] = '\0';
	while (fgets(command, sizeof(command), fp))
		strncat(line, command, sizeof(line) - strlen(line) - 1);
	assert_non_null(strstr(line, "id=1"));
	assert_non_null(strstr(line, "blob_value=b'blob-value'"));
	assert_non_null(strstr(line, "text_value=text-value"));
	assert_non_null(strstr(line, "int32_value=32"));
	assert_non_null(strstr(line, "int64_value=64"));
	assert_int_equal(fclose(fp), 0);
	assert_int_equal(unlink(output), 0);
}
