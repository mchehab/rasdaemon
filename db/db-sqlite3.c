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

#include "core/modules.h"
#include "core/ras-logger.h"

#include "db/ras-db.h"
#include "db/ras-db-backend.h"
#include "db/db-sqlite3.h"

#define DEBUG_SQL

/* Store the DB name on a static var to be used later on logs */
static char *full_fname = NULL;

static int db_sqlite3_open(struct ras_db **__db, void *__conn_parms,
			   unsigned int cpu)
{
	struct db_sqlite3_conn_params *conn_parms = __conn_parms;
	const char *fname = RAS_DB_FNAME;
	const char *dir = RASSTATEDIR;
	struct ras_record_priv *priv;
	sqlite3 **db = (void *)__db;
	struct stat st = {0};
	int flags, rc;

	flags = SQLITE_OPEN_FULLMUTEX |
		SQLITE_OPEN_READWRITE |
		SQLITE_OPEN_CREATE;

	/*
	 * Allow overriding default values with private parameters
	 */
	if (conn_parms) {
		if (conn_parms->fname)
			fname = conn_parms->fname;
		if (conn_parms->dir)
			dir = conn_parms->dir;
		if (conn_parms->extra_flags)
			flags |= conn_parms->extra_flags;
	}

	if (asprintf(&full_fname, "%s/%s", dir, fname) < 0) {
		log(TERM, LOG_ERR,
		    "Failed to create sqlite3 filename. Memory full?");
		return -1;
	}

	if (stat(dir, &st) == -1) {
		if (errno != ENOENT) {
			log(TERM, LOG_ERR,
			    "Failed to read state directory %s", dir);
			return -1;
		}

		if (mkdir(dir, 0700) == -1) {
			log(TERM, LOG_ERR,
			    "Failed to create state directory %s", dir);
			return -1;
		}
	}

	rc = sqlite3_initialize();
	if (rc != SQLITE_OK) {
		log(TERM, LOG_ERR,
		    "cpu %u: Failed to initialize sqlite: %s (error %d)\n",
		    cpu, sqlite3_errstr(rc), rc);
		return -1;
	}

	do {
		rc = sqlite3_open_v2(full_fname, db, flags, NULL);
		if (rc == SQLITE_BUSY)
			usleep(10000);
	} while (rc == SQLITE_BUSY);

	if (rc != SQLITE_OK) {
		log(TERM, LOG_ERR,
		    "cpu %u: Failed to connect to %s: %s (error %d)\n",
		    cpu, full_fname, sqlite3_errstr(rc), rc);
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
		    "cpu %u: Failed to close sqlite: %s (error %d)\n", cpu,
		    sqlite3_errstr(rc), rc);

	rc = sqlite3_shutdown();
	if (rc != SQLITE_OK)
		log(TERM, LOG_ERR,
		    "cpu %u: Failed to shutdown sqlite: %s (error %d)\n", cpu,
		    sqlite3_errstr(rc), rc);

	return rc;
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
	const char *str;

	switch (type) {
		case DB_TYPE_SERIAL:
			/* Use NULL to let sqlite3 to autofill it */
			sqlite3_bind_null(stmt, pos);
			break;

		case DB_TYPE_INT32:
			sqlite3_bind_int(stmt, pos, value);
			break;

		case DB_TYPE_INT64:
			sqlite3_bind_int64(stmt, pos, value);
			break;

		case DB_TYPE_TIMESTAMP:
		case DB_TYPE_TEXT:
		case DB_TYPE_BLOB:
		default:
			if (!value)
				sqlite3_bind_null(stmt, pos);
			else
				sqlite3_bind_text(stmt, pos, (const char *)value,
						  len, SQLITE_TRANSIENT);
			break;
	}
}

static void db_sqlite3_bind(const struct db_table_descriptor *db_tab,
			    struct ras_stmt *stmt,
			    const int pos, uint64_t value, int len)
{
	const struct db_fields *fields = db_tab->fields;
	int field_pos = 0;

	if (pos < 1) {
		log(TERM, LOG_INFO, "table %s: invalid pos: %d\n",
		    db_tab->name, pos);
		return;
	}

	for (int i = 0; i < db_tab->num_fields; i++) {
		if (fields[i].type == DB_TYPE_SERIAL)
			continue;

		if (++field_pos == pos)
			break;
	}

	db_bind_type(stmt, fields[field_pos].type, pos, value, len);
}

static int db_sqlite3_eval_stmt(struct ras_stmt *__stmt, const char *tab_name)
{
	sqlite3_stmt *stmt = (void *)__stmt;
	int rc;

	rc = sqlite3_step(stmt);
	if (rc != SQLITE_DONE)
		log(TERM, LOG_ERR,
		    "Failed to do step on sqlite. Table = %s: %s (error %d)\n",
		    tab_name, sqlite3_errstr(rc), rc);

	rc = sqlite3_reset(stmt);
	if (rc != SQLITE_OK)
		log(TERM, LOG_ERR,
		    "Failed to reset on sqlite. Table = %s: %s (error %d)\n",
	tab_name, sqlite3_errstr(rc), rc);

	rc = sqlite3_clear_bindings(stmt);
	if (rc != SQLITE_OK && rc != SQLITE_DONE)
		log(TERM, LOG_ERR,
		    "Failed to clear bindings on sqlite. Table = %s: %s (error %d)\n",
	tab_name, rc);

	return rc;
}

static int db_sqlite3_exec_sql(struct ras_db *__db, const char *sql)
{
	sqlite3		*db = (void *)__db;
	int rc;

#ifdef DEBUG_SQL
	log(TERM, LOG_INFO, "SQL: %s\n", sql);
	ras_logger_flush();
#endif

	rc = sqlite3_exec(db, sql, NULL, NULL, NULL);
	if (rc != SQLITE_OK) {
		log(TERM, LOG_ERR,
		    "Failed to exec '%s': %s (error %d)\n",
		    sql, sqlite3_errstr(rc), rc);
	}

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

	return db_sqlite3_exec_sql(__db, sql);
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
		    "Failed to query fields from the table %s on %s: : %s (error %d)\n",
		    db_tab->name, full_fname, sqlite3_errstr(rc), rc);
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
			int ret;
			type = db_get_sql_type(field->type, field->is_pk);

			/* add new field */
			p += snprintf(p, end - p, "ALTER TABLE %s ADD ",
				      db_tab->name);
			p += snprintf(p, end - p,
				      "%s %s", field->name, type);

			ret = db_sqlite3_exec_sql(__db, sql);
			if (ret)
				rc = ret;

			p = sql;
			*p = '\0';
		}
	}

	return rc;
}

static int __db_prepare_insert_stmt(struct sqlite3 *db,
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

	p += snprintf(p, end - p, ") VALUES (");

	for (i = 0; i < db_tab->num_fields; i++) {
		if (db_tab->fields[i].type == DB_TYPE_SERIAL)
			strscat(sql, "NULL", sizeof(sql));
		else
			strscat(sql, "?", sizeof(sql));

		if (i <  db_tab->num_fields - 1)
			strscat(sql, ", ", sizeof(sql));
		else
			strscat(sql, ")", sizeof(sql));
	}

#ifdef DEBUG_SQL
	log(TERM, LOG_INFO, "SQL: %s\n", sql);
	ras_logger_flush();
#endif

	rc = sqlite3_prepare_v2(db, sql, -1, stmt, NULL);
	if (rc != SQLITE_OK) {
		log(TERM, LOG_ERR,
		    "Failed to prepare insert db at table %s (db %s): error = %s\n",
		    db_tab->name, full_fname, sqlite3_errmsg(db));
		stmt = NULL;
	} else {
		log(TERM, LOG_INFO, "Recording %s events\n", db_tab->name);
	}

	return rc;
}

static int db_sqlite3_prepare_insert_stmt(struct ras_db *__db,
					  struct ras_stmt **__stmt,
					  const struct db_table_descriptor *db_tab)
{
	sqlite3_stmt **stmt = (sqlite3_stmt **)__stmt;
	sqlite3 *db = (sqlite3 *)__db;
	int rc;

	rc = __db_prepare_insert_stmt(db, stmt, db_tab);
	if (rc != SQLITE_OK) {
		log(TERM, LOG_ERR,
		    "Failed to prepare insert db at table %s (db %s): error = %s\n",
		    db_tab->name, full_fname, sqlite3_errmsg(db));

		log(TERM, LOG_INFO, "Trying to alter db at table %s (db %s)\n",
		    db_tab->name, full_fname);

		rc = db_alter_table(__db, __stmt, db_tab);
		if (rc != SQLITE_OK && rc != SQLITE_DONE) {
			log(TERM, LOG_ERR,
			    "Failed to alter db at table %s (db %s): error = %s\n",
			    db_tab->name, full_fname, sqlite3_errmsg(db));
			stmt = NULL;
			return rc;
		}

		rc = __db_prepare_insert_stmt(db, stmt, db_tab);
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
		    "cpu %u: Failed to finalize %s. Sqlite:  %s (error %d)\n",
		    cpu, name, sqlite3_errstr(rc), rc);
	else
		log(TERM, LOG_ERR,
		    "Failed to finalize sqlite: error = %d\n", rc);

	return rc;
}

/*
 * Database register data and init code
 */

static const struct ras_db_backend_ops sqlite3_backend_ops = {
	.open                   = db_sqlite3_open,
	.close                  = db_sqlite3_close,

	.get_sql_type           = db_sqlite3_get_sql_type,
	.bind_type              = db_sqlite3_bind_type,
	.bind                   = db_sqlite3_bind,

	.db_exec_sql		= db_sqlite3_exec_sql,

	.eval_stmt              = db_sqlite3_eval_stmt,
	.create_table           = db_sqlite3_create_table,
	.alter_table            = db_sqlite3_alter_table,
	.prepare_stmt           = db_sqlite3_prepare_insert_stmt,
	.finalize               = db_sqlite3_finalize,
};

static struct ras_db_backend_entry sqlite3_backend_entry = {
	.name = "sqlite3",
	.ops  = &sqlite3_backend_ops,
};

static int sqlite3_init(const char *name, struct ras_events *ras, void **priv)
{
	int ret;

	ret = db_backend_register(&sqlite3_backend_entry);
	if (ret != 0) {
		log(TERM, LOG_ERR, "Failed to init SQLite3 backend: %d\n", ret);
	} else {
		log(TERM, LOG_INFO, "SQLite3 DB backend initialized.\n");
	}

	return ret;
}

/*
 * Module auto-register data and code
 */

const struct ras_module_entry db_sqlite3_module = {
	.name = "db-sqlite3",
	.init = sqlite3_init,
	.level = DB_MODULE,
};

/*
 * Automatically register the module.
 */

__attribute__((constructor)) void sqlite3_register(void)
{
	int ret;

	ret = module_register(&db_sqlite3_module);
	if (ret != 0) {
		log(TERM, LOG_ERR, "Failed to register SQLite3 module", ret);
	} else {
		log(TERM, LOG_INFO, "SQLite3 backend registered successfully.\n");
	}
}
