/*
 * Copyright (C) 2013 Mauro Carvalho Chehab <mchehab+redhat@kernel.org>
 * Copyright (c) 2016, The Linux Foundation. All rights reserved.
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
#include "ras-aer-handler.h"
#include "ras-mce-handler.h"
#include "ras-logger.h"

/* #define DEBUG_SQL 1 */

#define SQLITE_RAS_DB RASSTATEDIR "/" RAS_DB_FNAME

/*
 * Table and functions to handle ras:mc_event
 */

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

int ras_store_mc_event(struct ras_events *ras, struct ras_mc_event *ev)
{
	int rc;
	struct sqlite3_priv *priv = ras->db_priv;

	if (!priv || !priv->stmt_mc_event)
		return 0;
	log(TERM, LOG_INFO, "mc_event store: %p\n", priv->stmt_mc_event);

	sqlite3_bind_text(priv->stmt_mc_event,  1, ev->timestamp, -1, NULL);
	sqlite3_bind_int (priv->stmt_mc_event,  2, ev->error_count);
	sqlite3_bind_text(priv->stmt_mc_event,  3, ev->error_type, -1, NULL);
	sqlite3_bind_text(priv->stmt_mc_event,  4, ev->msg, -1, NULL);
	sqlite3_bind_text(priv->stmt_mc_event,  5, ev->label, -1, NULL);
	sqlite3_bind_int (priv->stmt_mc_event,  6, ev->mc_index);
	sqlite3_bind_int (priv->stmt_mc_event,  7, ev->top_layer);
	sqlite3_bind_int (priv->stmt_mc_event,  8, ev->middle_layer);
	sqlite3_bind_int (priv->stmt_mc_event,  9, ev->lower_layer);
	sqlite3_bind_int64 (priv->stmt_mc_event, 10, ev->address);
	sqlite3_bind_int64 (priv->stmt_mc_event, 11, ev->grain);
	sqlite3_bind_int64 (priv->stmt_mc_event, 12, ev->syndrome);
	sqlite3_bind_text(priv->stmt_mc_event, 13, ev->driver_detail, -1, NULL);
	rc = sqlite3_step(priv->stmt_mc_event);
	if (rc != SQLITE_OK && rc != SQLITE_DONE)
		log(TERM, LOG_ERR,
		    "Failed to do mc_event step on sqlite: error = %d\n", rc);
	rc = sqlite3_reset(priv->stmt_mc_event);
	if (rc != SQLITE_OK && rc != SQLITE_DONE)
		log(TERM, LOG_ERR,
		    "Failed reset mc_event on sqlite: error = %d\n",
		    rc);
	log(TERM, LOG_INFO, "register inserted at db\n");

	return rc;
}

/*
 * Table and functions to handle ras:aer
 */

#ifdef HAVE_AER
static const struct db_fields aer_event_fields[] = {
		{ .name="id",			.type="INTEGER PRIMARY KEY" },
		{ .name="timestamp",		.type="TEXT" },
		{ .name="dev_name",		.type="TEXT" },
		{ .name="err_type",		.type="TEXT" },
		{ .name="err_msg",		.type="TEXT" },
};

static const struct db_table_descriptor aer_event_tab = {
	.name = "aer_event",
	.fields = aer_event_fields,
	.num_fields = ARRAY_SIZE(aer_event_fields),
};

int ras_store_aer_event(struct ras_events *ras, struct ras_aer_event *ev)
{
	int rc;
	struct sqlite3_priv *priv = ras->db_priv;

	if (!priv || !priv->stmt_aer_event)
		return 0;
	log(TERM, LOG_INFO, "aer_event store: %p\n", priv->stmt_aer_event);

	sqlite3_bind_text(priv->stmt_aer_event,  1, ev->timestamp, -1, NULL);
	sqlite3_bind_text(priv->stmt_aer_event,  2, ev->dev_name, -1, NULL);
	sqlite3_bind_text(priv->stmt_aer_event,  3, ev->error_type, -1, NULL);
	sqlite3_bind_text(priv->stmt_aer_event,  4, ev->msg, -1, NULL);

	rc = sqlite3_step(priv->stmt_aer_event);
	if (rc != SQLITE_OK && rc != SQLITE_DONE)
		log(TERM, LOG_ERR,
		    "Failed to do aer_event step on sqlite: error = %d\n", rc);
	rc = sqlite3_reset(priv->stmt_aer_event);
	if (rc != SQLITE_OK && rc != SQLITE_DONE)
		log(TERM, LOG_ERR,
		    "Failed reset aer_event on sqlite: error = %d\n",
		    rc);
	log(TERM, LOG_INFO, "register inserted at db\n");

	return rc;
}
#endif

/*
 * Table and functions to handle ras:non standard
 */

#ifdef HAVE_NON_STANDARD
static const struct db_fields non_standard_event_fields[] = {
		{ .name="id",			.type="INTEGER PRIMARY KEY" },
		{ .name="timestamp",		.type="TEXT" },
		{ .name="sec_type",		.type="BLOB" },
		{ .name="fru_id",		.type="BLOB" },
		{ .name="fru_text",		.type="TEXT" },
		{ .name="severity",		.type="TEXT" },
		{ .name="error",		.type="BLOB" },
};

static const struct db_table_descriptor non_standard_event_tab = {
	.name = "non_standard_event",
	.fields = non_standard_event_fields,
	.num_fields = ARRAY_SIZE(non_standard_event_fields),
};

int ras_store_non_standard_record(struct ras_events *ras, struct ras_non_standard_event *ev)
{
	int rc;
	struct sqlite3_priv *priv = ras->db_priv;

	if (!priv || !priv->stmt_non_standard_record)
		return 0;
	log(TERM, LOG_INFO, "non_standard_event store: %p\n", priv->stmt_non_standard_record);

	sqlite3_bind_text (priv->stmt_non_standard_record,  1, ev->timestamp, -1, NULL);
	sqlite3_bind_blob (priv->stmt_non_standard_record,  2, ev->sec_type, -1, NULL);
	sqlite3_bind_blob (priv->stmt_non_standard_record,  3, ev->fru_id, 16, NULL);
	sqlite3_bind_text (priv->stmt_non_standard_record,  4, ev->fru_text, -1, NULL);
	sqlite3_bind_text (priv->stmt_non_standard_record,  5, ev->severity, -1, NULL);
	sqlite3_bind_blob (priv->stmt_non_standard_record,  6, ev->error, ev->length, NULL);

	rc = sqlite3_step(priv->stmt_non_standard_record);
	if (rc != SQLITE_OK && rc != SQLITE_DONE)
		log(TERM, LOG_ERR,
		    "Failed to do non_standard_event step on sqlite: error = %d\n", rc);
	rc = sqlite3_reset(priv->stmt_non_standard_record);
	if (rc != SQLITE_OK && rc != SQLITE_DONE)
		log(TERM, LOG_ERR,
		    "Failed reset non_standard_event on sqlite: error = %d\n", rc);
	log(TERM, LOG_INFO, "register inserted at db\n");

	return rc;
}
#endif

/*
 * Table and functions to handle ras:arm
 */

#ifdef HAVE_ARM
static const struct db_fields arm_event_fields[] = {
		{ .name="id",			.type="INTEGER PRIMARY KEY" },
		{ .name="timestamp",		.type="TEXT" },
		{ .name="error_count",		.type="INTEGER" },
		{ .name="affinity",		.type="INTEGER" },
		{ .name="mpidr",		.type="INTEGER" },
		{ .name="running_state",	.type="INTEGER" },
		{ .name="psci_state",		.type="INTEGER" },
};

static const struct db_table_descriptor arm_event_tab = {
	.name = "arm_event",
	.fields = arm_event_fields,
	.num_fields = ARRAY_SIZE(arm_event_fields),
};

int ras_store_arm_record(struct ras_events *ras, struct ras_arm_event *ev)
{
	int rc;
	struct sqlite3_priv *priv = ras->db_priv;

	if (!priv || !priv->stmt_arm_record)
		return 0;
	log(TERM, LOG_INFO, "arm_event store: %p\n", priv->stmt_arm_record);

	sqlite3_bind_text (priv->stmt_arm_record,  1,  ev->timestamp, -1, NULL);
	sqlite3_bind_int  (priv->stmt_arm_record,  2,  ev->error_count);
	sqlite3_bind_int  (priv->stmt_arm_record,  3,  ev->affinity);
	sqlite3_bind_int64  (priv->stmt_arm_record,  4,  ev->mpidr);
	sqlite3_bind_int  (priv->stmt_arm_record,  5,  ev->running_state);
	sqlite3_bind_int  (priv->stmt_arm_record,  6,  ev->psci_state);

	rc = sqlite3_step(priv->stmt_arm_record);
	if (rc != SQLITE_OK && rc != SQLITE_DONE)
		log(TERM, LOG_ERR,
		    "Failed to do arm_event step on sqlite: error = %d\n", rc);
	rc = sqlite3_reset(priv->stmt_arm_record);
	if (rc != SQLITE_OK && rc != SQLITE_DONE)
		log(TERM, LOG_ERR,
		    "Failed reset arm_event on sqlite: error = %d\n",
		    rc);
	log(TERM, LOG_INFO, "register inserted at db\n");

	return rc;
}
#endif

#ifdef HAVE_EXTLOG
static const struct db_fields extlog_event_fields[] = {
		{ .name="id",			.type="INTEGER PRIMARY KEY" },
		{ .name="timestamp",		.type="TEXT" },
		{ .name="etype",		.type="INTEGER" },
		{ .name="error_count",		.type="INTEGER" },
		{ .name="severity",		.type="INTEGER" },
		{ .name="address",		.type="INTEGER" },
		{ .name="fru_id",		.type="BLOB" },
		{ .name="fru_text",		.type="TEXT" },
		{ .name="cper_data",		.type="BLOB" },
};

static const struct db_table_descriptor extlog_event_tab = {
	.name = "extlog_event",
	.fields = extlog_event_fields,
	.num_fields = ARRAY_SIZE(extlog_event_fields),
};

int ras_store_extlog_mem_record(struct ras_events *ras, struct ras_extlog_event *ev)
{
	int rc;
	struct sqlite3_priv *priv = ras->db_priv;

	if (!priv || !priv->stmt_extlog_record)
		return 0;
	log(TERM, LOG_INFO, "extlog_record store: %p\n", priv->stmt_extlog_record);

	sqlite3_bind_text  (priv->stmt_extlog_record,  1, ev->timestamp, -1, NULL);
	sqlite3_bind_int   (priv->stmt_extlog_record,  2, ev->etype);
	sqlite3_bind_int   (priv->stmt_extlog_record,  3, ev->error_seq);
	sqlite3_bind_int   (priv->stmt_extlog_record,  4, ev->severity);
	sqlite3_bind_int64 (priv->stmt_extlog_record,  5, ev->address);
	sqlite3_bind_blob  (priv->stmt_extlog_record,  6, ev->fru_id, 16, NULL);
	sqlite3_bind_text  (priv->stmt_extlog_record,  7, ev->fru_text, -1, NULL);
	sqlite3_bind_blob  (priv->stmt_extlog_record,  8, ev->cper_data, ev->cper_data_length, NULL);

	rc = sqlite3_step(priv->stmt_extlog_record);
	if (rc != SQLITE_OK && rc != SQLITE_DONE)
		log(TERM, LOG_ERR,
		    "Failed to do extlog_mem_record step on sqlite: error = %d\n", rc);
	rc = sqlite3_reset(priv->stmt_extlog_record);
	if (rc != SQLITE_OK && rc != SQLITE_DONE)
		log(TERM, LOG_ERR,
		    "Failed reset extlog_mem_record on sqlite: error = %d\n",
		    rc);
	log(TERM, LOG_INFO, "register inserted at db\n");

	return rc;
}
#endif

/*
 * Table and functions to handle mce:mce_record
 */

#ifdef HAVE_MCE
static const struct db_fields mce_record_fields[] = {
		{ .name="id",			.type="INTEGER PRIMARY KEY" },
		{ .name="timestamp",		.type="TEXT" },

		/* MCE registers */
		{ .name="mcgcap",		.type="INTEGER" },
		{ .name="mcgstatus",		.type="INTEGER" },
		{ .name="status",		.type="INTEGER" },
		{ .name="addr",			.type="INTEGER" }, // 5
		{ .name="misc",			.type="INTEGER" },
		{ .name="ip",			.type="INTEGER" },
		{ .name="tsc",			.type="INTEGER" },
		{ .name="walltime",		.type="INTEGER" },
		{ .name="cpu",			.type="INTEGER" }, // 10
		{ .name="cpuid",		.type="INTEGER" },
		{ .name="apicid",		.type="INTEGER" },
		{ .name="socketid",		.type="INTEGER" },
		{ .name="cs",			.type="INTEGER" },
		{ .name="bank",			.type="INTEGER" }, //15
		{ .name="cpuvendor",		.type="INTEGER" },

		/* Parsed data - will likely change */
		{ .name="bank_name",		.type="TEXT" },
		{ .name="error_msg",		.type="TEXT" },
		{ .name="mcgstatus_msg",	.type="TEXT" },
		{ .name="mcistatus_msg",	.type="TEXT" }, // 20
		{ .name="mcastatus_msg",	.type="TEXT" },
		{ .name="user_action",		.type="TEXT" },
		{ .name="mc_location",		.type="TEXT" },
};

static const struct db_table_descriptor mce_record_tab = {
	.name = "mce_record",
	.fields = mce_record_fields,
	.num_fields = ARRAY_SIZE(mce_record_fields),
};

int ras_store_mce_record(struct ras_events *ras, struct mce_event *ev)
{
	int rc;
	struct sqlite3_priv *priv = ras->db_priv;

	if (!priv || !priv->stmt_mce_record)
		return 0;
	log(TERM, LOG_INFO, "mce_record store: %p\n", priv->stmt_mce_record);

	sqlite3_bind_text  (priv->stmt_mce_record,  1, ev->timestamp, -1, NULL);
	sqlite3_bind_int   (priv->stmt_mce_record,  2, ev->mcgcap);
	sqlite3_bind_int   (priv->stmt_mce_record,  3, ev->mcgstatus);
	sqlite3_bind_int64 (priv->stmt_mce_record,  4, ev->status);
	sqlite3_bind_int64 (priv->stmt_mce_record,  5, ev->addr);
	sqlite3_bind_int64 (priv->stmt_mce_record,  6, ev->misc);
	sqlite3_bind_int64 (priv->stmt_mce_record,  7, ev->ip);
	sqlite3_bind_int64 (priv->stmt_mce_record,  8, ev->tsc);
	sqlite3_bind_int64 (priv->stmt_mce_record,  9, ev->walltime);
	sqlite3_bind_int   (priv->stmt_mce_record, 10, ev->cpu);
	sqlite3_bind_int   (priv->stmt_mce_record, 11, ev->cpuid);
	sqlite3_bind_int   (priv->stmt_mce_record, 12, ev->apicid);
	sqlite3_bind_int   (priv->stmt_mce_record, 13, ev->socketid);
	sqlite3_bind_int   (priv->stmt_mce_record, 14, ev->cs);
	sqlite3_bind_int   (priv->stmt_mce_record, 15, ev->bank);
	sqlite3_bind_int   (priv->stmt_mce_record, 16, ev->cpuvendor);

	sqlite3_bind_text(priv->stmt_mce_record, 17, ev->bank_name, -1, NULL);
	sqlite3_bind_text(priv->stmt_mce_record, 18, ev->error_msg, -1, NULL);
	sqlite3_bind_text(priv->stmt_mce_record, 19, ev->mcgstatus_msg, -1, NULL);
	sqlite3_bind_text(priv->stmt_mce_record, 20, ev->mcistatus_msg, -1, NULL);
	sqlite3_bind_text(priv->stmt_mce_record, 21, ev->mcastatus_msg, -1, NULL);
	sqlite3_bind_text(priv->stmt_mce_record, 22, ev->user_action, -1, NULL);
	sqlite3_bind_text(priv->stmt_mce_record, 23, ev->mc_location, -1, NULL);

	rc = sqlite3_step(priv->stmt_mce_record);
	if (rc != SQLITE_OK && rc != SQLITE_DONE)
		log(TERM, LOG_ERR,
		    "Failed to do mce_record step on sqlite: error = %d\n", rc);
	rc = sqlite3_reset(priv->stmt_mce_record);
	if (rc != SQLITE_OK && rc != SQLITE_DONE)
		log(TERM, LOG_ERR,
		    "Failed reset mce_record on sqlite: error = %d\n",
		    rc);
	log(TERM, LOG_INFO, "register inserted at db\n");

	return rc;
}
#endif

/*
 * Table and functions to handle devlink:devlink_health_report
 */

#ifdef HAVE_DEVLINK
static const struct db_fields devlink_event_fields[] = {
		{ .name="id",			.type="INTEGER PRIMARY KEY" },
		{ .name="timestamp",		.type="TEXT" },
		{ .name="bus_name",		.type="TEXT" },
		{ .name="dev_name",		.type="TEXT" },
		{ .name="driver_name",		.type="TEXT" },
		{ .name="reporter_name",	.type="TEXT" },
		{ .name="msg",			.type="TEXT" },
};

static const struct db_table_descriptor devlink_event_tab = {
	.name = "devlink_event",
	.fields = devlink_event_fields,
	.num_fields = ARRAY_SIZE(devlink_event_fields),
};

int ras_store_devlink_event(struct ras_events *ras, struct devlink_event *ev)
{
	int rc;
	struct sqlite3_priv *priv = ras->db_priv;

	if (!priv || !priv->stmt_devlink_event)
		return 0;
	log(TERM, LOG_INFO, "devlink_event store: %p\n", priv->stmt_devlink_event);

	sqlite3_bind_text(priv->stmt_devlink_event,  1, ev->timestamp, -1, NULL);
	sqlite3_bind_text(priv->stmt_devlink_event,  2, ev->bus_name, -1, NULL);
	sqlite3_bind_text(priv->stmt_devlink_event,  3, ev->dev_name, -1, NULL);
	sqlite3_bind_text(priv->stmt_devlink_event,  4, ev->driver_name, -1, NULL);
	sqlite3_bind_text(priv->stmt_devlink_event,  5, ev->reporter_name, -1, NULL);
	sqlite3_bind_text(priv->stmt_devlink_event,  6, ev->msg, -1, NULL);

	rc = sqlite3_step(priv->stmt_devlink_event);
	if (rc != SQLITE_OK && rc != SQLITE_DONE)
		log(TERM, LOG_ERR,
		    "Failed to do devlink_event step on sqlite: error = %d\n", rc);
	rc = sqlite3_reset(priv->stmt_devlink_event);
	if (rc != SQLITE_OK && rc != SQLITE_DONE)
		log(TERM, LOG_ERR,
		    "Failed reset devlink_event on sqlite: error = %d\n",
		    rc);
	log(TERM, LOG_INFO, "register inserted at db\n");

	return rc;
}
#endif

/*
 * Table and functions to handle block:block_rq_complete
 */

#ifdef HAVE_DISKERROR
static const struct db_fields diskerror_event_fields[] = {
		{ .name="id",			.type="INTEGER PRIMARY KEY" },
		{ .name="timestamp",		.type="TEXT" },
		{ .name="dev",			.type="TEXT" },
		{ .name="sector",		.type="INTEGER" },
		{ .name="nr_sector",		.type="INTEGER" },
		{ .name="error",		.type="TEXT" },
		{ .name="rwbs",			.type="TEXT" },
		{ .name="cmd",			.type="TEXT" },
};

static const struct db_table_descriptor diskerror_event_tab = {
	.name = "disk_errors",
	.fields = diskerror_event_fields,
	.num_fields = ARRAY_SIZE(diskerror_event_fields),
};

int ras_store_diskerror_event(struct ras_events *ras, struct diskerror_event *ev)
{
	int rc;
	struct sqlite3_priv *priv = ras->db_priv;

	if (!priv || !priv->stmt_diskerror_event)
		return 0;
	log(TERM, LOG_INFO, "diskerror_eventstore: %p\n", priv->stmt_diskerror_event);

	sqlite3_bind_text(priv->stmt_diskerror_event,  1, ev->timestamp, -1, NULL);
	sqlite3_bind_text(priv->stmt_diskerror_event,  2, ev->dev, -1, NULL);
	sqlite3_bind_int64(priv->stmt_diskerror_event,  3, ev->sector);
	sqlite3_bind_int(priv->stmt_diskerror_event,  4, ev->nr_sector);
	sqlite3_bind_text(priv->stmt_diskerror_event,  5, ev->error, -1, NULL);
	sqlite3_bind_text(priv->stmt_diskerror_event,  6, ev->rwbs, -1, NULL);
	sqlite3_bind_text(priv->stmt_diskerror_event,  7, ev->cmd, -1, NULL);

	rc = sqlite3_step(priv->stmt_diskerror_event);
	if (rc != SQLITE_OK && rc != SQLITE_DONE)
		log(TERM, LOG_ERR,
		    "Failed to do diskerror_event step on sqlite: error = %d\n", rc);
	rc = sqlite3_reset(priv->stmt_diskerror_event);
	if (rc != SQLITE_OK && rc != SQLITE_DONE)
		log(TERM, LOG_ERR,
		    "Failed reset diskerror_event on sqlite: error = %d\n",
		    rc);
	log(TERM, LOG_INFO, "register inserted at db\n");

	return rc;
}
#endif

/*
 * Generic code
 */
static int __ras_mc_prepare_stmt(struct sqlite3_priv *priv,
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
	if (rc != SQLITE_OK) {
		log(TERM, LOG_ERR,
		    "Failed to prepare insert db at table %s (db %s): error = %s\n",
		    db_tab->name, SQLITE_RAS_DB, sqlite3_errmsg(priv->db));
		stmt = NULL;
	} else {
		log(TERM, LOG_INFO, "Recording %s events\n", db_tab->name);
	}

	return rc;
}

static int ras_mc_create_table(struct sqlite3_priv *priv,
			       const struct db_table_descriptor *db_tab)
{
	const struct db_fields *field;
	char sql[1024], *p = sql, *end = sql + sizeof(sql);
	int i, rc;

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

static int ras_mc_alter_table(struct sqlite3_priv *priv,
			      sqlite3_stmt **stmt,
			      const struct db_table_descriptor *db_tab)
{
	char sql[1024], *p = sql, *end = sql + sizeof(sql);
	const struct db_fields *field;
	int col_count;
	int i, j, rc, found;

	snprintf(p, end - p, "SELECT * FROM %s", db_tab->name);
	rc = sqlite3_prepare_v2(priv->db, sql, -1, stmt, NULL);
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
			/* add new field */
			p += snprintf(p, end - p, "ALTER TABLE %s ADD ",
				      db_tab->name);
			p += snprintf(p, end - p,
				      "%s %s", field->name, field->type);
#ifdef DEBUG_SQL
			log(TERM, LOG_INFO, "SQL: %s\n", sql);
#endif
			rc = sqlite3_exec(priv->db, sql, NULL, NULL, NULL);
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

static int ras_mc_prepare_stmt(struct sqlite3_priv *priv,
			       sqlite3_stmt **stmt,
			       const struct db_table_descriptor *db_tab)
{
	int rc;

	rc = __ras_mc_prepare_stmt(priv, stmt, db_tab);
	if (rc != SQLITE_OK) {
		log(TERM, LOG_ERR,
		    "Failed to prepare insert db at table %s (db %s): error = %s\n",
		    db_tab->name, SQLITE_RAS_DB, sqlite3_errmsg(priv->db));

		log(TERM, LOG_INFO, "Trying to alter db at table %s (db %s)\n",
		    db_tab->name, SQLITE_RAS_DB);

		rc = ras_mc_alter_table(priv, stmt, db_tab);
		if (rc != SQLITE_OK && rc != SQLITE_DONE) {
			log(TERM, LOG_ERR,
			    "Failed to alter db at table %s (db %s): error = %s\n",
			    db_tab->name, SQLITE_RAS_DB,
			    sqlite3_errmsg(priv->db));
			stmt = NULL;
			return rc;
		}

		rc = __ras_mc_prepare_stmt(priv, stmt, db_tab);
	}

	return rc;
}

int ras_mc_add_vendor_table(struct ras_events *ras,
			    sqlite3_stmt **stmt,
			    const struct db_table_descriptor *db_tab)
{
	int rc;
	struct sqlite3_priv *priv = ras->db_priv;

	if (!priv)
		return -1;

	rc = ras_mc_create_table(priv, db_tab);
	if (rc == SQLITE_OK)
		rc = ras_mc_prepare_stmt(priv, stmt, db_tab);

	return rc;
}

int ras_mc_finalize_vendor_table(sqlite3_stmt *stmt)
{
	int rc;

	rc = sqlite3_finalize(stmt);
	if (rc != SQLITE_OK)
		log(TERM, LOG_ERR,
		    "Failed to finalize sqlite: error = %d\n", rc);

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
	priv->db = db;

	rc = ras_mc_create_table(priv, &mc_event_tab);
	if (rc == SQLITE_OK) {
		rc = ras_mc_prepare_stmt(priv, &priv->stmt_mc_event,
					 &mc_event_tab);
		if (rc != SQLITE_OK)
			goto error;
	}

#ifdef HAVE_AER
	rc = ras_mc_create_table(priv, &aer_event_tab);
	if (rc == SQLITE_OK) {
		rc = ras_mc_prepare_stmt(priv, &priv->stmt_aer_event,
					 &aer_event_tab);
		if (rc != SQLITE_OK)
			goto error;
	}
#endif

#ifdef HAVE_EXTLOG
	rc = ras_mc_create_table(priv, &extlog_event_tab);
	if (rc == SQLITE_OK) {
		rc = ras_mc_prepare_stmt(priv, &priv->stmt_extlog_record,
					 &extlog_event_tab);
		if (rc != SQLITE_OK)
			goto error;
	}
#endif

#ifdef HAVE_MCE
	rc = ras_mc_create_table(priv, &mce_record_tab);
	if (rc == SQLITE_OK) {
		rc = ras_mc_prepare_stmt(priv, &priv->stmt_mce_record,
					 &mce_record_tab);
		if (rc != SQLITE_OK)
			goto error;
	}
#endif

#ifdef HAVE_NON_STANDARD
	rc = ras_mc_create_table(priv, &non_standard_event_tab);
	if (rc == SQLITE_OK) {
		rc = ras_mc_prepare_stmt(priv, &priv->stmt_non_standard_record,
					&non_standard_event_tab);
		if (rc != SQLITE_OK)
			goto error;
	}
#endif

#ifdef HAVE_ARM
	rc = ras_mc_create_table(priv, &arm_event_tab);
	if (rc == SQLITE_OK) {
		rc = ras_mc_prepare_stmt(priv, &priv->stmt_arm_record,
					&arm_event_tab);
		if (rc != SQLITE_OK)
			goto error;
	}
#endif
#ifdef HAVE_DEVLINK
	rc = ras_mc_create_table(priv, &devlink_event_tab);
	if (rc == SQLITE_OK) {
		rc = ras_mc_prepare_stmt(priv, &priv->stmt_devlink_event,
					&devlink_event_tab);
		if (rc != SQLITE_OK)
			goto error;
	}
#endif

#ifdef HAVE_DISKERROR
	rc = ras_mc_create_table(priv, &diskerror_event_tab);
	if (rc == SQLITE_OK) {
		rc = ras_mc_prepare_stmt(priv, &priv->stmt_diskerror_event,
					&diskerror_event_tab);
		if (rc != SQLITE_OK)
			goto error;
	}
#endif

	ras->db_priv = priv;
	return 0;

error:
	free(priv);
	return -1;
}

int ras_mc_event_closedb(unsigned int cpu, struct ras_events *ras)
{
	int rc;
	sqlite3 *db;
	struct sqlite3_priv *priv = ras->db_priv;

	printf("Calling %s()\n", __func__);

	if (!priv)
		return -1;

	db = priv->db;
	if (!db)
		return -1;

	if (priv->stmt_mc_event) {
		rc = sqlite3_finalize(priv->stmt_mc_event);
		if (rc != SQLITE_OK)
			log(TERM, LOG_ERR,
			    "cpu %u: Failed to finalize mc_event sqlite: error = %d\n",
			    cpu, rc);
	}

#ifdef HAVE_AER
	if (priv->stmt_aer_event) {
		rc = sqlite3_finalize(priv->stmt_aer_event);
		if (rc != SQLITE_OK)
			log(TERM, LOG_ERR,
			    "cpu %u: Failed to finalize aer_event sqlite: error = %d\n",
			    cpu, rc);
	}
#endif

#ifdef HAVE_EXTLOG
	if (priv->stmt_extlog_record) {
		rc = sqlite3_finalize(priv->stmt_extlog_record);
		if (rc != SQLITE_OK)
			log(TERM, LOG_ERR,
			    "cpu %u: Failed to finalize extlog_record sqlite: error = %d\n",
			    cpu, rc);
	}
#endif


#ifdef HAVE_MCE
	if (priv->stmt_mce_record) {
		rc = sqlite3_finalize(priv->stmt_mce_record);
		if (rc != SQLITE_OK)
			log(TERM, LOG_ERR,
			    "cpu %u: Failed to finalize mce_record sqlite: error = %d\n",
			    cpu, rc);
	}
#endif

#ifdef HAVE_NON_STANDARD
	if (priv->stmt_non_standard_record) {
		rc = sqlite3_finalize(priv->stmt_non_standard_record);
		if (rc != SQLITE_OK)
			log(TERM, LOG_ERR,
			    "cpu %u: Failed to finalize non_standard_record sqlite: error = %d\n",
			    cpu, rc);
	}
#endif

#ifdef HAVE_ARM
	if (priv->stmt_arm_record) {
		rc = sqlite3_finalize(priv->stmt_arm_record);
		if (rc != SQLITE_OK)
			log(TERM, LOG_ERR,
			    "cpu %u: Failed to finalize arm_record sqlite: error = %d\n",
			    cpu, rc);
	}
#endif

#ifdef HAVE_DEVLINK
	if (priv->stmt_devlink_event) {
		rc = sqlite3_finalize(priv->stmt_devlink_event);
		if (rc != SQLITE_OK)
			log(TERM, LOG_ERR,
			    "cpu %u: Failed to finalize devlink_event sqlite: error = %d\n",
			    cpu, rc);
	}
#endif

#ifdef HAVE_DISKERROR
	if (priv->stmt_diskerror_event) {
		rc = sqlite3_finalize(priv->stmt_diskerror_event);
		if (rc != SQLITE_OK)
			log(TERM, LOG_ERR,
			    "cpu %u: Failed to finalize diskerror_event sqlite: error = %d\n",
			    cpu, rc);
	}
#endif

	rc = sqlite3_close_v2(db);
	if (rc != SQLITE_OK)
		log(TERM, LOG_ERR,
		    "cpu %u: Failed to close sqlite: error = %d\n", cpu, rc);

	rc = sqlite3_shutdown();
	if (rc != SQLITE_OK)
		log(TERM, LOG_ERR,
		    "cpu %u: Failed to shutdown sqlite: error = %d\n", cpu, rc);
	free(priv);

	return 0;
}
