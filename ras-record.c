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

#include "ras-mc-event.h"
#include <string.h>
#include <stdio.h>

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

sqlite3 *ras_mc_event_opendb(struct ras_events *ras)
{
	int rc, i;
	sqlite3 *db;
	char sql[1024];

	rc = sqlite3_initialize();
	if (rc != SQLITE_OK) {
		printf("Failed to initialize sqlite: error = %d\n", rc);
		return NULL;
	}

	rc = sqlite3_open_v2(SQLITE_RAS_DB, &db,
			     SQLITE_OPEN_FULLMUTEX | SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL);
	if (rc != SQLITE_OK) {
		printf("Failed to connect to %s: error = %d\n",
		       SQLITE_RAS_DB, rc);
		return NULL;
	}

	strcpy(sql, createdb);
	strcat(sql, mc_event_db);
	strcat(sql, mc_event_db_create_fields);
printf("%s\n", sql);
	rc = sqlite3_exec(db, sql, NULL, NULL, NULL);
	if (rc != SQLITE_OK) {
		printf("Failed to create db on %s: error = %d\n",
		       SQLITE_RAS_DB, rc);

		return NULL;
	}

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
printf("%s\n", sql);
	rc = sqlite3_prepare_v2(db, sql, -1, &ras->stmt, NULL);
	if (rc != SQLITE_OK) {
		printf("Failed to prepare insert db on %s: error = %s\n",
		       SQLITE_RAS_DB, sqlite3_errmsg(db));
		return NULL;
	}

	return db;
}

int ras_store_mc_event(struct ras_events *ras, struct ras_mc_event *ev)
{
	int rc;

	sqlite3_bind_text(ras->stmt,  1, ev->timestamp, -1, NULL);
	sqlite3_bind_int (ras->stmt,  2, ev->error_count);
	sqlite3_bind_text(ras->stmt,  3, ev->error_type, -1, NULL);
	sqlite3_bind_text(ras->stmt,  4, ev->msg, -1, NULL);
	sqlite3_bind_text(ras->stmt,  5, ev->label, -1, NULL);
	sqlite3_bind_int (ras->stmt,  6, ev->mc_index);
	sqlite3_bind_int (ras->stmt,  7, ev->top_layer);
	sqlite3_bind_int (ras->stmt,  8, ev->middle_layer);
	sqlite3_bind_int (ras->stmt,  9, ev->lower_layer);
	sqlite3_bind_int (ras->stmt, 10, ev->address);
	sqlite3_bind_int (ras->stmt, 11, ev->grain);
	sqlite3_bind_int (ras->stmt, 12, ev->syndrome);
	sqlite3_bind_text(ras->stmt, 13, ev->driver_detail, -1, NULL);
	rc = sqlite3_step(ras->stmt);
	if (rc != SQLITE_OK && rc != SQLITE_DONE)
		printf("Failed to do step on sqlite: error = %d\n", rc);
	rc = sqlite3_finalize(ras->stmt);
	if (rc != SQLITE_OK && rc != SQLITE_DONE)
		printf("Failed to do finalize insert on sqlite: error = %d\n",
		       rc);

	return rc;
}
