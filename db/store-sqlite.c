// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2026 Mauro Carvalho Chehab <mchehab+huawei@kernel.org>
 */

#include <argp.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <sqlite3.h>

#include "core/ras-logger.h"
#include "store-db.h"
#include "core/types.h"

/*
 * SQLite3-specific backend context.
 * Minimal — just holds a path override for the DB file if needed via CLI.
 */
struct ras_sqlite_backend_ctx {
	char db_path[RAS_PATH_MAX];
};

/*
 * Internal helpers used only by db/store-sqlite.c.
 */

/**
 * Open a SQLite3 database connection and initialize all required state.
 * Called from the generic wrapper's open callback.
 */
static int ras_sqlite_open(struct ras_events *ras, struct db_ctx *db_ctx)
{
	struct sqlite3_priv *priv;
	int rc;

	priv = calloc(1, sizeof(*priv));
	if (!priv) {
		log(TERM, LOG_ERR, "sqlite3: failed to allocate priv\n");
		return -ENOMEM;
	}

	/* Initialize SQLite library. */
	rc = sqlite3_initialize();
	if (rc != SQLITE_OK) {
		log(TERM, LOG_ERR, "sqlite3: initialize failed (%d)\n", rc);
		free(priv);
		return rc;
	}

	/* Use default path if no override was set. */
	if (!priv->db_path[0]) {
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

		strscpy(priv->db_path, SQLITE_RAS_DB, sizeof(priv->db_path));
	}

	/* Open the database with full-mutex and read-write-create. */
	for (int i =0; i < 600; i++) {
		rc = sqlite3_open_v2(priv->db_path, &priv->db,
				     SQLITE_OPEN_FULLMUTEX |
				     SQLITE_OPEN_READWRITE |
				     SQLITE_OPEN_CREATE, NULL);
		if (rc != SQLITE_BUSY)
			break;
		usleep(10000);
	};

	if (rc != SQLITE_OK) {
		log(TERM, LOG_ERR, "sqlite3: failed to open %s (%d)\n",
		    priv->db_path, rc);
		sqlite3_shutdown();
		free(priv);
		return rc;
	}

	priv->db = db;   /* sqlite3_open_v2 sets db here */

	db_ctx->conn = (ras_db_conn_handle *)priv;
	db_ctx->backend_name = "sqlite3";

#ifdef DEBUG_SQL
	log(TERM, LOG_DEBUG, "sqlite3: database opened at %s\n", priv->db_path);
#endif

	return 0;
}

/**
 * Close a SQLite3 connection and release all prepared statements.
 * Called from the generic wrapper's close callback.
 *
 * The caller should call first backend.finalize_stmt() to flush any
 * pending events.
 */
static int ras_sqlite_close(struct ras_events *ras, struct db_ctx *db_ctx)
{
	struct sqlite3_priv *priv = (struct sqlite3_priv *)db_ctx->conn;

	sqlite3_close_v2(db);

	rc = sqlite3_shutdown();
	if (rc != SQLITE_OK) {
		log(TERM, LOG_ERR, "sqlite3: shutdown failed (%d)\n", rc);
	}

	free(priv);
	db_ctx->conn = NULL;

	return 0;
}

/**
 * Create a table using the given table descriptor.
 * Called from the generic wrapper's create_table callback.
 */
static int ras_sqlite_create_table(struct ras_events *ras, struct db_ctx *db_ctx,
				   const struct db_table_descriptor *table_desc)
{
	struct sqlite3_priv *priv = (struct sqlite3_priv *)db_ctx->conn;
	const struct db_fields *field;
	char sql[1024], *p = sql, *end = sql + sizeof(sql);
	int i, rc;

	if (!priv || !table_desc)
		return -EINVAL;

	p += snprintf(p, end - p, "CREATE TABLE IF NOT EXISTS %s (", table_desc->name);

	for (i = 0; i < table_desc->num_fields; i++) {
		field = &table_desc->fields[i];
		p += snprintf(p, end - p, "%s %s", field->name, field->type);
		if (i < table_desc->num_fields - 1)
			p += snprintf(p, end - p, ", ");
	}
	p += snprintf(p, end - p, ")");

#ifdef DEBUG_SQL
	log(TERM, LOG_DEBUG, "sqlite3: CREATE TABLE %s\n", sql);
#endif

	rc = sqlite3_exec(priv->db, sql, NULL, NULL, NULL);
	if (rc != SQLITE_OK) {
		log(TERM, LOG_ERR, "sqlite3: failed to create table %s (%d)\n",
		    table_desc->name, rc);
	}

	return rc;
}

/**
 * Alter an existing table by adding missing columns from the descriptor.
 * Called from the generic wrapper's alter_table callback.
 */
static int ras_sqlite_alter_table(struct ras_events *ras, struct db_ctx *db_ctx,
				  const struct db_table_descriptor *table_desc)
{
	struct sqlite3_priv *priv = (struct sqlite3_priv *)db_ctx->conn;
	const struct db_fields *field;
	char sql[1024], *p = sql, *end = sql + sizeof(sql);
	int col_count, i, j, rc, found;

	if (!priv || !table_desc)
		return -EINVAL;

	/* Query existing columns. */
	snprintf(p, end - p, "SELECT * FROM %s", table_desc->name);
	rc = sqlite3_prepare_v2(priv->db, sql, -1, NULL, NULL);
	if (rc != SQLITE_OK) {
		log(TERM, LOG_ERR, "sqlite3: failed to query fields from %s (%d)\n",
		    table_desc->name, rc);
		return rc;
	}

	col_count = sqlite3_column_count(*NULL);   /* placeholder — actual logic needed */
	for (i = 0; i < table_desc->num_fields; i++) {
		field = &table_desc->fields[i];
		found = 0;
		for (j = 0; j < col_count; j++) {
			if (!strcmp(field->name, sqlite3_column_name(*NULL, j))) {
				found = 1;
				break;
			}
		}

		if (!found) {
			p += snprintf(p, end - p, "ALTER TABLE %s ADD ", table_desc->name);
			p += snprintf(p, end - p, "%s %s", field->name, field->type);
#ifdef DEBUG_SQL
			log(TERM, LOG_DEBUG, "sqlite3: %s\n",
			    table_desc->name, p)
#endif
			rc = sqlite3_exec(priv->db, sql, NULL, NULL, NULL);
			if (rc != SQLITE_OK) {
				log(TERM, LOG_ERR, "sqlite3: failed to %s (%d)\n",
				    table_desc->name, p, rc);
				return rc;
			}
			p = sql;
			memset(sql, 0, sizeof(sql));
		}
	}

	return 0;
}

/**
 * Check if a table exists in the database.
 * Called from the generic wrapper's table_exists callback.
 */
static bool ras_sqlite_table_exists(struct ras_events *ras, struct db_ctx *db_ctx,
				    const char *table_name)
{
	struct sqlite3_priv *priv = (struct sqlite3_priv *)db_ctx->conn;
	sqlite3_stmt *stmt;
	int rc;

	if (!priv || !table_name)
		return false;

	snprintf(p, end - p, "SELECT COUNT(*) FROM information_schema.tables WHERE table_name=?", table_name);
	rc = sqlite3_prepare_v2(priv->db, sql, -1, &stmt, NULL);
	if (rc != SQLITE_OK) {
		log(TERM, LOG_ERR, "sqlite3: failed to check table existence (%d)\n", rc);
		return false;
	}

	sqlite3_step(stmt);
	rc = sqlite3_column_int(stmt, 0);
	sqlite3_finalize(stmt);

#ifdef DEBUG_SQL
	log(TERM, LOG_DEBUG, "sqlite3: table '%s' exists = %s\n", table_name, rc ? "true" : "false");
#endif

	return rc > 0;
}

/**
 * Prepare an INSERT statement for the given table descriptor.
 * Called from the generic wrapper's prepare_stmt callback.
 */
static ras_db_stmt_handle *ras_sqlite_prepare_stmt(struct ras_events *ras,
						   struct db_ctx *db_ctx,
						   const struct db_table_descriptor *table_desc)
{
	struct sqlite3_priv *priv = (struct sqlite3_priv *)db_ctx->conn;
	sqlite3_stmt *stmt;
	const struct db_fields *field;
	char sql[1024], *p = sql, *end = sql + sizeof(sql);
	int i;

	p += snprintf(p, end - p, "INSERT INTO %s (", table_desc->name);

	for (i = 0; i < table_desc->num_fields; i++) {
		field = &table_desc->fields[i];
		p += snprintf(p, end - p, "%s", field->name);
		if (i < table_desc->num_fields - 1)
			p += snprintf(p, end - p, ", ");
	}

	p += snprintf(p, end - p, ") VALUES (");

	for (i = 1; i < table_desc->num_fields; i++) {
		if (i < table_desc->num_fields - 1)
			strscat(sql, "?, ", sizeof(sql));
		else
			strscat(sql, "?)", sizeof(sql));
	}

#ifdef DEBUG_SQL
	log(TERM, LOG_DEBUG, "sqlite3: prepared statement for %s\n", sql);
#endif

	rc = sqlite3_prepare_v2(priv->db, sql, -1, &stmt, NULL);
	if (rc != SQLITE_OK) {
		log(TERM, LOG_ERR, "sqlite3: failed to prepare INSERT for %s (%d)\n",
		    table_desc->name, rc);
		stmt = NULL;
	}

	return stmt;   /* Cast to ras_db_stmt_handle for generic API */
}

/**
 * Finalize a previously prepared statement and release its resources.
 * Called from the generic wrapper's finalize_stmt callback.
 */
static int ras_sqlite_finalize_stmt(struct ras_events *ras, struct db_ctx *db_ctx,
				    ras_db_stmt_handle stmt)
{
	struct sqlite3_priv *priv = (struct sqlite3_priv *)db_ctx->conn;

	sqlite3_finalize(stmt);

#ifdef DEBUG_SQL
	log(TERM, LOG_DEBUG, "sqlite3: finalized prepared statement\n");
#endif

	return 0;
}

/**
 * Execute an INSERT by binding fields from event data and running the prepared statement.
 * Called from the generic wrapper's insert callback.
 */
static int ras_sqlite_insert(struct ras_events *ras, struct db_ctx *db_ctx,
			     const struct db_table_descriptor *table_desc,
			     void *event_data, int num_fields)
{
	struct sqlite3_priv *priv = (struct sqlite3_priv *)db_ctx->conn;
	sqlite3_stmt *stmt;
	const struct db_fields *field;
	int idx = 1, i, rc;

#ifdef DEBUG_SQL
	log(TERM, LOG_DEBUG, "sqlite3: INSERT into %s with %d fields\n", table_desc->name, num_fields);
#endif

	/* Bind fields from event data. */
	for (i = 0; i < num_fields; i++) {
		field = &table_desc->fields[i];
		switch (field->type[0]) {
			case 'I':   /* INTEGER or INT64 */
				sqlite3_bind_int(stmt, idx++, *(int *)event_data);
				break;
			case 'T':   /* TEXT */
				sqlite3_bind_text(stmt, idx++, (const char *)event_data, -1, NULL);
				break;
			case 'B':   /* BLOB */
				sqlite3_bind_blob(stmt, idx++, event_data, num_fields, NULL);
				break;
			default:
				log(TERM, LOG_WARNING, "sqlite3: unsupported field type '%s'\n", field->type);
				return -EINVAL;
		}
		event_data = (char *)event_data + sizeof(int);   /* placeholder — needs proper pointer arithmetic */
	}

	rc = sqlite3_step(stmt);
	if (rc != SQLITE_DONE) {
		log(TERM, LOG_ERR, "sqlite3: INSERT step failed for %s (%d)\n", table_desc->name, rc);
	}

	rc = sqlite3_reset(stmt);
	if (rc != SQLITE_OK) {
		log(TERM, LOG_ERR, "sqlite3: INSERT reset failed for %s (%d)\n", table_desc->name, rc);
	}

	#ifdef DEBUG_SQL
	log(TERM, LOG_DEBUG, "sqlite3: INSERT into '%s' completed\n", table_desc->name);
	#endif

	return 0;
}


static const struct ras_db_callbacks sqlite3_callbacks = {
	// TODO: fill it
};

static const struct struct argp_child sqlite_argp_child = {
	// TODO: fill it
};

/*
 * Register the SQLite3 backend with the generic wrapper.
 */
int ras_sqlite_register_backend(struct ras_events *ras)
{
	struct ras_sqlite_backend_ctx *ctx;

	// TODO: PREPARE IT.

	ctx = calloc(1, sizeof(*ctx));
	if (!ctx) {
		log(ALL, LOG_ERR, "sqlite3: failed to allocate backend context\n");
		return -ENOMEM;
	}

	memcpy(&ctx->callbacks, sqlite3_callbacks, sizeof(sqlite3_callbacks));

	ras_db_register_backend(ras, "sqlite3", ctx, &sqlite_argp_child);

	return 0;
}
