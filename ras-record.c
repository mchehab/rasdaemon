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

#define SQLITE_RAS_DB	"ras-mc_event.db"

const char *mc_event_db = " mc_event ";
const char *mc_event_db_create_fields = "("
		"id INTEGER PRIMARY KEY"
		", timestamp TEXT"
		", err_count INTEGER"
		", err_type TEXT"
		", err_msg TEXT"		/* 5 */
		", label TEXT"
		", mc INTEGER"
		", top_layer INTEGER"
		", middle_layer INTEGER"
		", lower_layer INTEGER"		/* 10 */
		", address INTEGER"
		", grain INTEGER"
		", syndrome INTEGER"
		", driver_detail TEXT"		/* 14 */
	")";

const char *mc_event_db_fields = "("
		"id"
		", timestamp"
		", err_count"
		", err_type"
		", err_msg"			/* 5 */
		", label"
		", mc"
		", top_layer"
		", middle_layer"
		", lower_layer"			/* 10 */
		", address"
		", grain"
		", syndrome"
		", driver_detail"		/* 14 */
	")";

#define NUM_MC_EVENT_DB_VALUES	14

const char *createdb = "CREATE TABLE IF NOT EXISTS";
const char *insertdb = "INSERT INTO";
const char *valuesdb = " VALUES ";

static int ras_mc_prepare_stmt(struct sqlite3_priv *priv)
{
	int i, rc;
	char sql[1024];

	strcpy(sql, insertdb);
	strcat(sql, mc_event_db);
	strcat(sql, mc_event_db_fields);
	strcat(sql, valuesdb);

	strcat(sql, "(NULL, ");	/* Auto-increment field */
	for (i = 1; i < NUM_MC_EVENT_DB_VALUES; i++) {
		if (i < NUM_MC_EVENT_DB_VALUES - 1)
			strcat(sql, "?, ");
		else
			strcat(sql, "?)");
	}

	rc = sqlite3_prepare_v2(priv->db, sql, -1, &priv->stmt, NULL);
	if (rc != SQLITE_OK)
		log(TERM, LOG_ERR, "Failed to prepare insert db on %s: error = %s\n",
		       SQLITE_RAS_DB, sqlite3_errmsg(priv->db));

	return rc;
}

int ras_mc_event_opendb(unsigned cpu, struct ras_events *ras)
{
	int rc, i;
	sqlite3 *db;
	char sql[1024];
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

	strcpy(sql, createdb);
	strcat(sql, mc_event_db);
	strcat(sql, mc_event_db_create_fields);
	rc = sqlite3_exec(db, sql, NULL, NULL, NULL);
	if (rc != SQLITE_OK) {
		log(TERM, LOG_ERR,
		    "cpu %u: Failed to create db on %s: error = %d\n",
		    cpu, SQLITE_RAS_DB, rc);
		free(priv);
		return -1;
	}

	priv->db = db;
	ras->db_priv = priv;

	rc = ras_mc_prepare_stmt(priv);
	if (rc == SQLITE_OK)
		log(TERM, LOG_INFO,
		    "cpu %u: Recording events at %s\n",
		    cpu, SQLITE_RAS_DB, rc);

	return 0;
}

int ras_store_mc_event(struct ras_events *ras, struct ras_mc_event *ev)
{
	int rc;
	struct sqlite3_priv *priv = ras->db_priv;

	log(TERM, LOG_INFO, "store_event: %p\n", priv->stmt);
	if (!priv->stmt)
		return 0;

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
		log(TERM, LOG_ERR, "Failed to do step on sqlite: error = %d\n", rc);
	rc = sqlite3_finalize(priv->stmt);
	if (rc != SQLITE_OK && rc != SQLITE_DONE)
		log(TERM, LOG_ERR, "Failed to do finalize insert on sqlite: error = %d\n",
		       rc);
	log(TERM, LOG_INFO, "register interted at db\n");

	return rc;
}
