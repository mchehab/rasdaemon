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
#include "core/ras-events.h"
#include "db/ras-db.h"

#define SQLITE_RAS_DB RASSTATEDIR "/" RAS_DB_FNAME

const char *db_get_sql_type(enum db_field_type type, bool is_pk)
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

void ras_store_bind_type(struct ras_stmt *__stmt, const enum db_field_type type,
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

void ras_store_bind(struct ras_stmt *stmt, const struct db_fields *fields,
		    const int pos, uint64_t value, int len)
{
	if (pos < 1) {
		log(TERM, LOG_INFO, "invalid pos: %d\n", pos);
		return;
	}

	ras_store_bind_type(stmt, fields[pos - 1].type, pos, value, len);
}

int ras_store_eval_stmt(struct ras_stmt *__stmt, const char *tab_name)
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

int ras_mc_create_table(struct ras_db *__db,
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

int ras_mc_alter_table(struct ras_db *__db,
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

static int __ras_mc_prepare_stmt(struct sqlite3 *db,
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

int ras_mc_prepare_stmt(struct ras_db *__db,
			struct ras_stmt **__stmt,
			const struct db_table_descriptor *db_tab)
{
	sqlite3_stmt **stmt = (sqlite3_stmt **)__stmt;
	sqlite3 *db = (sqlite3 *)__db;
	int rc;

	rc = __ras_mc_prepare_stmt(db, stmt, db_tab);
	if (rc != SQLITE_OK) {
		log(TERM, LOG_ERR,
		    "Failed to prepare insert db at table %s (db %s): error = %s\n",
		    db_tab->name, SQLITE_RAS_DB, sqlite3_errmsg(db));

		log(TERM, LOG_INFO, "Trying to alter db at table %s (db %s)\n",
		    db_tab->name, SQLITE_RAS_DB);

		rc = ras_mc_alter_table(__db, __stmt, db_tab);
		if (rc != SQLITE_OK && rc != SQLITE_DONE) {
			log(TERM, LOG_ERR,
			    "Failed to alter db at table %s (db %s): error = %s\n",
			    db_tab->name, SQLITE_RAS_DB,
       sqlite3_errmsg(db));
			stmt = NULL;
			return rc;
		}

		rc = __ras_mc_prepare_stmt(db, stmt, db_tab);
	}

	return rc;
}

// TODO: remove struct ras_events *ras, replacing it by struct ras_db
int ras_mc_add_vendor_table(struct ras_events *ras,
			    struct ras_stmt **stmt,
			    const struct db_table_descriptor *db_tab)
{
	struct ras_db *db = ras->db;
	int rc;

	rc = ras_mc_create_table(db, db_tab);
	if (rc == SQLITE_OK)
		rc = ras_mc_prepare_stmt(db, stmt, db_tab);

	/*
	 * on sqlite3, SQLITE_OK is actually zero, but let's do it to
	 * stabilish a generic API contract: returning zero here means no
	 * error.
	 */
	if (rc == SQLITE_OK)
		return 0;

	return rc;
}

int ras_mc_finalize_vendor_table(struct ras_stmt *__stmt)
{
	sqlite3_stmt *stmt = (void *)__stmt;
	int rc;

	rc = sqlite3_finalize(stmt);
	if (rc != SQLITE_OK)
		log(TERM, LOG_ERR,
		    "Failed to finalize sqlite: error = %d\n", rc);

		return rc;
}

int ras_mc_opendb(unsigned int cpu, struct ras_events *ras, size_t size_priv)
{
	sqlite3 *db = (void *) ras->db;
	struct ras_record_priv *priv;
	int rc;

	ras->db_ref_count++;
	if (ras->db_ref_count > 1)
		return 0;

	ras->db_priv = NULL;

	priv = calloc(1, size_priv);
	if (!priv)
		return -1;

	struct stat st = {0};

	if (stat(RASSTATEDIR, &st) == -1) {
		if (errno != ENOENT) {
			log(TERM, LOG_ERR,
			    "Failed to read state directory " RASSTATEDIR);
			goto error;
		}

		if (mkdir(RASSTATEDIR, 0700) == -1) {
			log(TERM, LOG_ERR,
			    "Failed to create state directory " RASSTATEDIR);
			goto error;
		}
	}

	rc = sqlite3_initialize();
	if (rc != SQLITE_OK) {
		log(TERM, LOG_ERR,
		    "cpu %u: Failed to initialize sqlite: error = %d\n",
		    cpu, rc);
		goto error;
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
		goto error;
	}

	ras->db = (struct ras_db *)db;
	ras->db_priv = priv;
	return 0;

error:
	free(priv);
	return -1;
}

int ras_mc_finalize(unsigned int cpu, struct ras_stmt *__stmt, const char *name)
{
	sqlite3_stmt *stmt = (void *)__stmt;

	if (stmt) {
		int rc = sqlite3_finalize(stmt);
		if (rc != SQLITE_OK)
			log(TERM, LOG_ERR,
			    "cpu %u: Failed to finalize %s. Sqlite: error = %d\n",
                            cpu, name, rc);
	}
}

int ras_mc_closedb(unsigned int cpu, struct ras_events *ras)
{
	int rc;

	rc = sqlite3_close_v2((sqlite3 *)ras->db);
	if (rc != SQLITE_OK)
		log(TERM, LOG_ERR,
		    "cpu %u: Failed to close sqlite: error = %d\n", cpu, rc);

	rc = sqlite3_shutdown();
	if (rc != SQLITE_OK)
		log(TERM, LOG_ERR,
		    "cpu %u: Failed to shutdown sqlite: error = %d\n", cpu, rc);

	free(ras->db_priv);
	ras->db_priv = NULL;

	return 0;
}

