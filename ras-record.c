/*
 * Copyright (C) 2013 Mauro Carvalho Chehab <mchehab@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
*/

/*
 * BuildRequires: sqlite-devel
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "ras-events.h"
#include "ras-mc-handler.h"
#include "ras-logger.h"

/* #define DEBUG_SQL 1 */

#define SQLITE_RAS_DB RASSTATEDIR "/" RAS_DB_FNAME


#define ARRAY_SIZE(x) (sizeof(x)/sizeof(*(x)))

struct db_fields {
	char *name;
	char *type;
};

struct db_table_descriptor {
	char			*name;
	const struct db_fields	*fields;
	size_t			num_fields;
};

static const struct db_fields mc_event_fields[] = {
		{ .name="id",			.type="INTEGER PRIMARY KEY" },
		{ .name="timestamp",		.type="TEXT" },
		{ .name="err_count",		.type="INTEGER" },
		{ .name="err_type",		.type="TEXT" },
		{ .name="err_msg",		.type="TEXT" },
		{ .name="label",		.type="TEXT" },
		{ .name="mc",			.type="INTEGER" },
		{ .name="top_layer",		.type="INTEGER" },
		{ .name="middle_layer",		.type="INTEGER" },
		{ .name="lower_layer",		.type="INTEGER" },
		{ .name="address",		.type="INTEGER" },
		{ .name="grain",		.type="INTEGER" },
		{ .name="syndrome",		.type="INTEGER" },
		{ .name="driver_detail",	.type="TEXT" },
};

static const struct db_table_descriptor mc_event_tab = {
	.name = "mc_event",
	.fields = mc_event_fields,
	.num_fields = ARRAY_SIZE(mc_event_fields),
};

const char *insertdb = "INSERT INTO";
const char *valuesdb = " VALUES ";

static int ras_mc_prepare_stmt(struct sqlite3_priv *priv,
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
			strcat(sql, "?, ");
		else
			strcat(sql, "?)");
	}

#ifdef DEBUG_SQL
	log(TERM, LOG_INFO, "SQL: %s\n", sql);
#endif

	rc = sqlite3_prepare_v2(priv->db, sql, -1, stmt, NULL);
	if (rc != SQLITE_OK)
		log(TERM, LOG_ERR,
		    "Failed to prepare insert db at table %s (db %s): error = %s\n",
		    db_tab->name, SQLITE_RAS_DB, sqlite3_errmsg(priv->db));

	return rc;
}

static int ras_mc_create_table(struct sqlite3_priv *priv,
			       const struct db_table_descriptor *db_tab)
{
	const struct db_fields *field;
	char sql[1024], *p = sql, *end = sql + sizeof(sql);
	int i,rc;

	p += snprintf(p, end - p, "CREATE TABLE IF NOT EXISTS %s (",
		      db_tab->name);

	for (i = 0; i < db_tab->num_fields; i++) {
		field = &db_tab->fields[i];
		p += snprintf(p, end - p, "%s %s", field->name, field->type);

		if (i < db_tab->num_fields - 1)
			p += snprintf(p, end - p, ", ");
	}
	p += snprintf(p, end - p, ")");

#ifdef DEBUG_SQL
	log(TERM, LOG_INFO, "SQL: %s\n", sql);
#endif

	rc = sqlite3_exec(priv->db, sql, NULL, NULL, NULL);
	if (rc != SQLITE_OK) {
		log(TERM, LOG_ERR,
		    "Failed to create table %s on %s: error = %d\n",
		    db_tab->name, SQLITE_RAS_DB, rc);
	}
	return rc;
}

int ras_mc_event_opendb(unsigned cpu, struct ras_events *ras)
{
	int rc;
	sqlite3 *db;
	struct sqlite3_priv *priv;

	printf("Calling %s()\n", __FUNCTION__);

	ras->db_priv = NULL;

	priv = calloc(1, sizeof(*priv));
	if (!priv)
		return -1;

	rc = sqlite3_initialize();
	if (rc != SQLITE_OK) {
		log(TERM, LOG_ERR,
		    "cpu %u: Failed to initialize sqlite: error = %d\n",
		    cpu, rc);
		free(priv);
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
		free(priv);
		return -1;
	}
	priv->db = db;

	rc = ras_mc_create_table(priv, &mc_event_tab);
	if (rc != SQLITE_OK) {
		sqlite3_close(db);
		free(priv);
		return -1;
	}

	rc = ras_mc_prepare_stmt(priv, &priv->stmt, &mc_event_tab);
	if (rc == SQLITE_OK) {
		log(TERM, LOG_INFO,
		    "cpu %u: Recording events at %s\n",
		    cpu, SQLITE_RAS_DB);
		ras->db_priv = priv;
	} else {
		sqlite3_close(db);
		free(priv);
		return -1;
	}

	return 0;
}

int ras_store_mc_event(struct ras_events *ras, struct ras_mc_event *ev)
{
	int rc;
	struct sqlite3_priv *priv = ras->db_priv;

	if (!priv || !priv->stmt)
		return 0;
	log(TERM, LOG_INFO, "mc_event store: %p\n", priv->stmt);

	sqlite3_bind_text(priv->stmt,  1, ev->timestamp, -1, NULL);
	sqlite3_bind_int (priv->stmt,  2, ev->error_count);
	sqlite3_bind_text(priv->stmt,  3, ev->error_type, -1, NULL);
	sqlite3_bind_text(priv->stmt,  4, ev->msg, -1, NULL);
	sqlite3_bind_text(priv->stmt,  5, ev->label, -1, NULL);
	sqlite3_bind_int (priv->stmt,  6, ev->mc_index);
	sqlite3_bind_int (priv->stmt,  7, ev->top_layer);
	sqlite3_bind_int (priv->stmt,  8, ev->middle_layer);
	sqlite3_bind_int (priv->stmt,  9, ev->lower_layer);
	sqlite3_bind_int (priv->stmt, 10, ev->address);
	sqlite3_bind_int (priv->stmt, 11, ev->grain);
	sqlite3_bind_int (priv->stmt, 12, ev->syndrome);
	sqlite3_bind_text(priv->stmt, 13, ev->driver_detail, -1, NULL);
	rc = sqlite3_step(priv->stmt);
	if (rc != SQLITE_OK && rc != SQLITE_DONE)
		log(TERM, LOG_ERR, "Failed to do mc_event step on sqlite: error = %d\n", rc);
	rc = sqlite3_reset(priv->stmt);
	if (rc != SQLITE_OK && rc != SQLITE_DONE)
		log(TERM, LOG_ERR, "Failed reset mc_event on sqlite: error = %d\n",
		       rc);
	log(TERM, LOG_INFO, "register inserted at db\n");

	return rc;
}
