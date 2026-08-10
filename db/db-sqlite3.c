/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2026 Mauro Carvalho Chehab <mchehab+huawei@kernel.org>
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sqlite3.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "config.h"

#include "core/ras-logger.h"

#include "db/ras-db.h"
#include "db/ras-db-backend.h"

#define SQLITE_RAS_DB RASSTATEDIR "/" RAS_DB_FNAME

static int db_sqlite3_open(struct ras_db *__db, unsigned int cpu)
{
	sqlite3 *db = (void *) __db;
	struct ras_record_priv *priv;
	int rc;

	struct stat st = {0};

	if (stat(RASSTATEDIR, &st) == -1) {
		if (errno != ENOENT) {
			log(TERM, LOG_ERR,
			    "Failed to read state directory " RASSTATEDIR);
			return -1;
		}

		if (mkdir(RASSTATEDIR, 0700) == -1) {
			log(TERM, LOG_ERR,
			    "Failed to create state directory " RASSTATEDIR);
			return -1;
		}
	}

	rc = sqlite3_initialize();
	if (rc != SQLITE_OK) {
		log(TERM, LOG_ERR,
		    "cpu %u: Failed to initialize sqlite: error = %d\n",
		    cpu, rc);
		return -1;
	}

	do {
		rc = sqlite3_open_v2(SQLITE_RAS_DB, &db,
				     SQLITE_OPEN_FULLMUTEX |
				     SQLITE_OPEN_READWRITE |
				     SQLITE_OPEN_CREATE, NULL);
		if (rc == SQLITE_BUSY)
			usleep(10000);
	} while (rc == SQLITE_BUSY);

	if (rc != SQLITE_OK) {
		log(TERM, LOG_ERR,
		    "cpu %u: Failed to connect to %s: error = %d\n",
		    cpu, SQLITE_RAS_DB, rc);
		return -1;
	}

	return 0;
}

static int db_sqlite3_close(struct ras_db *__db, unsigned int cpu)
{
	sqlite3 *db = (void *) __db;
	int rc;

	rc = sqlite3_close_v2((sqlite3 *)db);
	if (rc != SQLITE_OK)
		log(TERM, LOG_ERR,
		    "cpu %u: Failed to close sqlite: error = %d\n", cpu, rc);

	rc = sqlite3_shutdown();
	if (rc != SQLITE_OK)
		log(TERM, LOG_ERR,
		    "cpu %u: Failed to shutdown sqlite: error = %d\n", cpu, rc);

	return 0;
}

static const char *db_sqlite3_get_sql_type(enum db_field_type type, bool is_pk)
{
	/*
	* On sqlite3, integers are 64 bits and there's no timestamp type
	*/
	switch (type) {
	case DB_TYPE_SERIAL:
	case DB_TYPE_INT64:
	case DB_TYPE_INT32:
		if (is_pk)
			return "INTEGER PRIMARY KEY";
		return "INTEGER";
	case DB_TYPE_TIMESTAMP:
	case DB_TYPE_TEXT:
		if (is_pk)
			return "TEXT PRIMARY KEY";
		return "TEXT";
	case DB_TYPE_BLOB:
	default:
		if (is_pk)
			return "BLOB PRIMARY KEY";
		return "BLOB";
	}
}

static void db_sqlite3_bind_type(struct ras_stmt *__stmt,
				 const enum db_field_type type,
				 const int pos, uint64_t value, int len)
{
	sqlite3_stmt *stmt = (void *)__stmt;

	switch (type) {
		case DB_TYPE_SERIAL:
		case DB_TYPE_INT32:
			sqlite3_bind_int(stmt, pos, value);
			break;

		case DB_TYPE_INT64:
			sqlite3_bind_int64(stmt, pos, value);
			break;

		case DB_TYPE_TIMESTAMP:
		case DB_TYPE_TEXT:
			sqlite3_bind_text(stmt, pos, (const char *)value,
					  len, SQLITE_TRANSIENT);
			break;

		case DB_TYPE_BLOB:
		default:
			sqlite3_bind_blob(stmt, pos, (const char *)value,
					  len, SQLITE_TRANSIENT);
	}
}

static void db_sqlite3_bind(struct ras_stmt *stmt,
			   const struct db_fields *fields,
			   const int pos, uint64_t value, int len)
{
	if (pos < 1) {
		log(TERM, LOG_INFO, "invalid pos: %d\n", pos);
		return;
	}

	db_bind_type(stmt, fields[pos - 1].type, pos, value, len);
}

static int db_sqlite3_eval_stmt(struct ras_stmt *__stmt, const char *tab_name)
{
	sqlite3_stmt *stmt = (void *)__stmt;
	int rc;

	rc = sqlite3_step(stmt);
	if (rc != SQLITE_DONE)
		log(TERM, LOG_ERR,
		"Failed to do step on sqlite. Table = %s error = %d\n",
	tab_name, rc);

	rc = sqlite3_reset(stmt);
	if (rc != SQLITE_OK)
		log(TERM, LOG_ERR,
		"Failed to reset on sqlite. Table = %s error = %d\n",
	tab_name, rc);

	rc = sqlite3_clear_bindings(stmt);
	if (rc != SQLITE_OK && rc != SQLITE_DONE)
		log(TERM, LOG_ERR,
		"Failed to clear bindings on sqlite. Table = %s error = %d\n",
	tab_name, rc);

	return rc;
}

static int db_sqlite3_create_table(struct ras_db *__db,
				   const struct db_table_descriptor *db_tab)
{
	char sql[1024], *p = sql, *end = sql + sizeof(sql);
	const struct db_fields *field;
	sqlite3		*db = (void *)__db;
	const char *type;
	int i, rc;

	p += snprintf(p, end - p, "CREATE TABLE IF NOT EXISTS %s (",
		      db_tab->name);

	for (i = 0; i < db_tab->num_fields; i++) {
		field = &db_tab->fields[i];
		type = db_get_sql_type(field->type, field->is_pk);

		p += snprintf(p, end - p, "%s %s", field->name, type);

		if (i < db_tab->num_fields - 1)
			p += snprintf(p, end - p, ", ");
	}
	p += snprintf(p, end - p, ")");

#ifdef DEBUG_SQL
	log(TERM, LOG_INFO, "SQL: %s\n", sql);
#endif

	rc = sqlite3_exec(db, sql, NULL, NULL, NULL);
	if (rc != SQLITE_OK) {
		log(TERM, LOG_ERR,
		    "Failed to create table %s on %s: error = %d\n",
		    db_tab->name, SQLITE_RAS_DB, rc);
	}
	return rc;
}

static int db_sqlite3_alter_table(struct ras_db *__db,
				  struct ras_stmt **__stmt,
				  const struct db_table_descriptor *db_tab)
{
	char sql[1024], *p = sql, *end = sql + sizeof(sql);
	sqlite3_stmt **stmt = (void *)__stmt;
	const struct db_fields *field;
	sqlite3 *db = (void *)__db;
	const char *type;
	int col_count;
	int i, j, rc, found;

	snprintf(p, end - p, "SELECT * FROM %s", db_tab->name);
	rc = sqlite3_prepare_v2(db, sql, -1, stmt, NULL);
	if (rc != SQLITE_OK) {
		log(TERM, LOG_ERR,
		    "Failed to query fields from the table %s on %s: error = %d\n",
		    db_tab->name, SQLITE_RAS_DB, rc);
		return rc;
	}

	col_count = sqlite3_column_count(*stmt);
	for (i = 0; i < db_tab->num_fields; i++) {
		field = &db_tab->fields[i];
		found = 0;
		for (j = 0; j < col_count; j++) {
			if (!strcmp(field->name,
				    sqlite3_column_name(*stmt, j))) {
				found = 1;
				break;
			}
		}

		if (!found) {
			type = db_get_sql_type(field->type, field->is_pk);

			/* add new field */
			p += snprintf(p, end - p, "ALTER TABLE %s ADD ",
				      db_tab->name);
			p += snprintf(p, end - p,
				      "%s %s", field->name, type);
#ifdef DEBUG_SQL
			log(TERM, LOG_INFO, "SQL: %s\n", sql);
#endif
			rc = sqlite3_exec(db, sql, NULL, NULL, NULL);
			if (rc != SQLITE_OK) {
				log(TERM, LOG_ERR,
				    "Failed to add new field %s to the table %s on %s: error = %d\n",
				    field->name, db_tab->name,
				    SQLITE_RAS_DB, rc);
				return rc;
			}
			p = sql;
			memset(sql, 0, sizeof(sql));
		}
	}

	return rc;
}

static int __db_prepare_stmt(struct sqlite3 *db,
			     sqlite3_stmt **stmt,
			     const struct db_table_descriptor *db_tab)

{
	int i, rc;
	char sql[1024], *p = sql, *end = sql + sizeof(sql);
	const struct db_fields *field;

	p += snprintf(p, end - p, "INSERT INTO %s (",
		      db_tab->name);

	for (i = 0; i < db_tab->num_fields; i++) {
		field = &db_tab->fields[i];
		p += snprintf(p, end - p, "%s", field->name);

		if (i < db_tab->num_fields - 1)
			p += snprintf(p, end - p, ", ");
	}

	p += snprintf(p, end - p, ") VALUES ( NULL, ");

	for (i = 1; i < db_tab->num_fields; i++) {
		if (i <  db_tab->num_fields - 1)
			strscat(sql, "?, ", sizeof(sql));
		else
			strscat(sql, "?)", sizeof(sql));
	}

	#ifdef DEBUG_SQL
	log(TERM, LOG_INFO, "SQL: %s\n", sql);
	#endif

	rc = sqlite3_prepare_v2(db, sql, -1, stmt, NULL);
	if (rc != SQLITE_OK) {
		log(TERM, LOG_ERR,
		    "Failed to prepare insert db at table %s (db %s): error = %s\n",
		    db_tab->name, SQLITE_RAS_DB, sqlite3_errmsg(db));
		stmt = NULL;
	} else {
		log(TERM, LOG_INFO, "Recording %s events\n", db_tab->name);
	}

	return rc;
}

static int db_sqlite3_prepare_stmt(struct ras_db *__db,
				   struct ras_stmt **__stmt,
				   const struct db_table_descriptor *db_tab)
{
	sqlite3_stmt **stmt = (sqlite3_stmt **)__stmt;
	sqlite3 *db = (sqlite3 *)__db;
	int rc;

	rc = __db_prepare_stmt(db, stmt, db_tab);
	if (rc != SQLITE_OK) {
		log(TERM, LOG_ERR,
		    "Failed to prepare insert db at table %s (db %s): error = %s\n",
		    db_tab->name, SQLITE_RAS_DB, sqlite3_errmsg(db));

		log(TERM, LOG_INFO, "Trying to alter db at table %s (db %s)\n",
		    db_tab->name, SQLITE_RAS_DB);

		rc = db_alter_table(__db, __stmt, db_tab);
		if (rc != SQLITE_OK && rc != SQLITE_DONE) {
			log(TERM, LOG_ERR,
			    "Failed to alter db at table %s (db %s): error = %s\n",
			    db_tab->name, SQLITE_RAS_DB,
       sqlite3_errmsg(db));
			stmt = NULL;
			return rc;
		}

		rc = __db_prepare_stmt(db, stmt, db_tab);
	}

	return rc;
}

static int db_sqlite3_finalize(unsigned int cpu,
			       struct ras_stmt *__stmt, const char *name)
{
	sqlite3_stmt *stmt = (void *)__stmt;
	int rc;

	rc = sqlite3_finalize(stmt);
	if (rc == SQLITE_OK)
		return 0;

	if (cpu >= 0)
		log(TERM, LOG_ERR,
		    "cpu %u: Failed to finalize %s. Sqlite: error = %d\n",
		    cpu, name, rc);
	else
		log(TERM, LOG_ERR,
		    "Failed to finalize sqlite: error = %d\n", rc);

	return rc;
}

static const struct ras_db_backend_ops sqlite3_backend_ops = {
	.open                   = db_sqlite3_open,
	.close                  = db_sqlite3_close,

	.get_sql_type           = db_sqlite3_get_sql_type,
	.bind_type              = db_sqlite3_bind_type,
	.bind                   = db_sqlite3_bind,

	.eval_stmt              = db_sqlite3_eval_stmt,
	.create_table           = db_sqlite3_create_table,
	.alter_table            = db_sqlite3_alter_table,
	.prepare_stmt           = db_sqlite3_prepare_stmt,
	.finalize               = db_sqlite3_finalize,
};

static struct ras_db_backend_entry sqlite3_backend_entry = {
	.name = "sqlite3",
	.ops  = &sqlite3_backend_ops,
};

/*
 * Automatically register the backend.
 */
__attribute__((constructor)) static void sqlite3_register_backend(void)
{
	int ret;

	ret = db_backend_register(&sqlite3_backend_entry);
	if (ret != 0) {
		log(TERM, LOG_ERR, "Failed to register SQLite3 backend: %d\n", ret);
	} else {
		log(TERM, LOG_INFO, "SQLite3 backend registered successfully.\n");
	}
}
