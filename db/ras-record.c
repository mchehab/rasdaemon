// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * Copyright (C) 2013 Mauro Carvalho Chehab <mchehab+huawei@kernel.org>
 * Copyright (c) 2016, The Linux Foundation. All rights reserved.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "modules/ras-aer-handler.h"
#include "core/ras-events.h"
#include "core/ras-logger.h"
#include "events-arch-x86/ras-mce-handler.h"
#include "core/ras-mc-handler.h"
#include "db/ras-record.h"
#include "events/ras-reri-handler.h"

/*
 * BuildRequires: sqlite-devel
 */

/* #define DEBUG_SQL 1 */

#define SQLITE_RAS_DB RASSTATEDIR "/" RAS_DB_FNAME

static inline const char *db_get_sql_type(enum db_field_type type, bool is_pk)
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

void ras_store_bind_type(sqlite3_stmt *stmt, const enum db_field_type type,
			 const int pos, uint64_t value, int len)
{
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

void ras_store_bind(sqlite3_stmt *stmt, const struct db_fields *fields,
		    const int pos, uint64_t value, int len)
{
	if (pos < 1) {
		log(TERM, LOG_INFO, "invalid pos: %d\n", pos);
		return;
	}

	ras_store_bind_type(stmt, fields[pos - 1].type, pos, value, len);
}

int ras_store_eval_stmt(struct sqlite3_stmt *stmt, const char *tab_name)
{
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

/*
 * Table and functions to handle ras:mc_event
 */

static const struct db_fields mc_event_fields[] = {
	{ .name = "id",			.type = DB_TYPE_SERIAL, .is_pk = true },
	{ .name = "timestamp",		.type = DB_TYPE_TIMESTAMP},
	{ .name = "err_count",		.type = DB_TYPE_INT32 },
	{ .name = "err_type",		.type = DB_TYPE_TEXT },
	{ .name = "err_msg",		.type = DB_TYPE_TEXT },
	{ .name = "label",		.type = DB_TYPE_TEXT },
	{ .name = "mc",			.type = DB_TYPE_INT32 },
	{ .name = "top_layer",		.type = DB_TYPE_INT32 },
	{ .name = "middle_layer",	.type = DB_TYPE_INT32 },
	{ .name = "lower_layer",	.type = DB_TYPE_INT32 },
	{ .name = "address",		.type = DB_TYPE_INT64 },
	{ .name = "grain",		.type = DB_TYPE_INT64 },
	{ .name = "syndrome",		.type = DB_TYPE_INT64 },
	{ .name = "driver_detail",	.type = DB_TYPE_TEXT },
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

	ras_store_bind(priv->stmt_mc_event, mc_event_fields,  1, (uint64_t)ev->timestamp, -1);
	ras_store_bind(priv->stmt_mc_event, mc_event_fields,  2, ev->error_count, -1);
	ras_store_bind(priv->stmt_mc_event, mc_event_fields,  3, (uint64_t)ev->error_type, -1);
	ras_store_bind(priv->stmt_mc_event, mc_event_fields,  4, (uint64_t)ev->msg, -1);
	ras_store_bind(priv->stmt_mc_event, mc_event_fields,  5, (uint64_t)ev->label, -1);
	ras_store_bind(priv->stmt_mc_event, mc_event_fields,  6, ev->mc_index, -1);
	ras_store_bind(priv->stmt_mc_event, mc_event_fields,  7, ev->top_layer, -1);
	ras_store_bind(priv->stmt_mc_event, mc_event_fields,  8, ev->middle_layer, -1);
	ras_store_bind(priv->stmt_mc_event, mc_event_fields,  9, ev->lower_layer, -1);
	ras_store_bind(priv->stmt_mc_event, mc_event_fields, 10, ev->address, -1);
	ras_store_bind(priv->stmt_mc_event, mc_event_fields, 11, ev->grain, -1);
	ras_store_bind(priv->stmt_mc_event, mc_event_fields, 12, ev->syndrome, -1);
	ras_store_bind(priv->stmt_mc_event, mc_event_fields, 13, (uint64_t)ev->driver_detail, -1);
	rc = sqlite3_step(priv->stmt_mc_event);
	if (rc != SQLITE_DONE)
		log(TERM, LOG_ERR,
		    "Failed to do mc_event step on sqlite: error = %d\n", rc);
	rc = sqlite3_reset(priv->stmt_mc_event);
	if (rc != SQLITE_OK)
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
	{ .name = "id",			.type = DB_TYPE_SERIAL, .is_pk = true },
	{ .name = "timestamp",		.type = DB_TYPE_TEXT },
	{ .name = "dev_name",		.type = DB_TYPE_TEXT },
	{ .name = "err_type",		.type = DB_TYPE_TEXT },
	{ .name = "err_msg",		.type = DB_TYPE_TEXT },
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

	ras_store_bind(priv->stmt_aer_event, aer_event_fields, 1, (uint64_t)ev->timestamp, -1);
	ras_store_bind(priv->stmt_aer_event, aer_event_fields, 2, (uint64_t)ev->dev_name, -1);
	ras_store_bind(priv->stmt_aer_event, aer_event_fields, 3, (uint64_t)ev->error_type, -1);
	ras_store_bind(priv->stmt_aer_event, aer_event_fields, 4, (uint64_t)ev->msg, -1);

	rc = sqlite3_step(priv->stmt_aer_event);
	if (rc != SQLITE_DONE)
		log(TERM, LOG_ERR,
		    "Failed to do aer_event step on sqlite: error = %d\n", rc);
	rc = sqlite3_reset(priv->stmt_aer_event);
	if (rc != SQLITE_OK)
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
		{ .name = "id",			.type = DB_TYPE_SERIAL, .is_pk = true },
		{ .name = "timestamp",		.type = DB_TYPE_TEXT },
		{ .name = "sec_type",		.type = DB_TYPE_BLOB },
		{ .name = "fru_id",		.type = DB_TYPE_BLOB },
		{ .name = "fru_text",		.type = DB_TYPE_TEXT },
		{ .name = "severity",		.type = DB_TYPE_TEXT },
		{ .name = "error",		.type = DB_TYPE_BLOB },
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

	ras_store_bind(priv->stmt_non_standard_record, non_standard_event_fields, 1, (uint64_t)ev->timestamp, -1);
	ras_store_bind(priv->stmt_non_standard_record, non_standard_event_fields, 2, (uint64_t)ev->sec_type, -1);
	ras_store_bind(priv->stmt_non_standard_record, non_standard_event_fields, 3, (uint64_t)ev->fru_id,  16);
	ras_store_bind(priv->stmt_non_standard_record, non_standard_event_fields, 4, (uint64_t)ev->fru_text, -1);
	ras_store_bind(priv->stmt_non_standard_record, non_standard_event_fields, 5, (uint64_t)ev->severity, -1);
	ras_store_bind(priv->stmt_non_standard_record, non_standard_event_fields, 6, (uint64_t)ev->error,  ev->length);

	rc = sqlite3_step(priv->stmt_non_standard_record);
	if (rc != SQLITE_DONE)
		log(TERM, LOG_ERR,
		    "Failed to do non_standard_event step on sqlite: error = %d\n", rc);
	rc = sqlite3_reset(priv->stmt_non_standard_record);
	if (rc != SQLITE_OK)
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
		{ .name = "id",			.type = DB_TYPE_SERIAL, .is_pk = true },
		{ .name = "timestamp",		.type = DB_TYPE_TEXT },
		{ .name = "error_count",	.type = DB_TYPE_INT32 },
		{ .name = "affinity",		.type = DB_TYPE_INT32 },
		{ .name = "mpidr",		.type = DB_TYPE_INT64 },
		{ .name = "running_state",	.type = DB_TYPE_INT32 },
		{ .name = "psci_state",		.type = DB_TYPE_INT32 },
		{ .name = "err_info",		.type = DB_TYPE_BLOB },
		{ .name = "context_info",	.type = DB_TYPE_BLOB },
		{ .name = "vendor_info",	.type = DB_TYPE_BLOB },
		{ .name = "error_type",		.type = DB_TYPE_TEXT },
		{ .name = "error_flags",	.type = DB_TYPE_TEXT },
		{ .name = "error_info",		.type = DB_TYPE_INT64 },
		{ .name = "virt_fault_addr",	.type = DB_TYPE_INT64 },
		{ .name = "phy_fault_addr",	.type = DB_TYPE_INT64 },
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

	ras_store_bind(priv->stmt_arm_record, arm_event_fields, 1, (uint64_t)ev->timestamp, -1);
	ras_store_bind(priv->stmt_arm_record, arm_event_fields, 2, ev->error_count, -1);
	ras_store_bind(priv->stmt_arm_record, arm_event_fields, 3, ev->affinity, -1);
	ras_store_bind(priv->stmt_arm_record, arm_event_fields, 4, ev->mpidr, -1);
	ras_store_bind(priv->stmt_arm_record, arm_event_fields, 5, ev->running_state, -1);
	ras_store_bind(priv->stmt_arm_record, arm_event_fields, 6, ev->psci_state, -1);
	ras_store_bind(priv->stmt_arm_record, arm_event_fields, 7, (uint64_t)ev->pei_error,  ev->pei_len);
	ras_store_bind(priv->stmt_arm_record, arm_event_fields, 8, (uint64_t)ev->ctx_error,  ev->ctx_len);
	ras_store_bind(priv->stmt_arm_record, arm_event_fields, 9, (uint64_t)ev->vsei_error,  ev->oem_len);
	ras_store_bind(priv->stmt_arm_record, arm_event_fields, 10, (uint64_t)ev->error_types, -1);
	ras_store_bind(priv->stmt_arm_record, arm_event_fields, 11, (uint64_t)ev->error_flags, -1);
	ras_store_bind(priv->stmt_arm_record, arm_event_fields, 12, ev->error_info, -1);
	ras_store_bind(priv->stmt_arm_record, arm_event_fields, 13, ev->virt_fault_addr, -1);
	ras_store_bind(priv->stmt_arm_record, arm_event_fields, 14, ev->phy_fault_addr, -1);

	rc = sqlite3_step(priv->stmt_arm_record);
	if (rc != SQLITE_DONE)
		log(TERM, LOG_ERR,
		    "Failed to do arm_event step on sqlite: error = %d\n", rc);
	rc = sqlite3_reset(priv->stmt_arm_record);
	if (rc != SQLITE_OK)
		log(TERM, LOG_ERR,
		    "Failed reset arm_event on sqlite: error = %d\n",
		    rc);
	log(TERM, LOG_INFO, "register inserted at db\n");

	return rc;
}
#endif

#ifdef HAVE_EXTLOG
static const struct db_fields extlog_event_fields[] = {
	{ .name = "id",			.type = DB_TYPE_SERIAL, .is_pk = true },
	{ .name = "timestamp",		.type = DB_TYPE_TEXT },
	{ .name = "etype",		.type = DB_TYPE_INT32 },
	{ .name = "error_count",	.type = DB_TYPE_INT32 },
	{ .name = "severity",		.type = DB_TYPE_INT32 },
	{ .name = "address",		.type = DB_TYPE_INT64 },
	{ .name = "fru_id",		.type = DB_TYPE_BLOB },
	{ .name = "fru_text",		.type = DB_TYPE_TEXT },
	{ .name = "cper_data",		.type = DB_TYPE_BLOB },
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

	ras_store_bind(priv->stmt_extlog_record, extlog_event_fields, 1, (uint64_t)ev->timestamp, -1);
	ras_store_bind(priv->stmt_extlog_record, extlog_event_fields, 2, ev->etype, -1);
	ras_store_bind(priv->stmt_extlog_record, extlog_event_fields, 3, ev->error_seq, -1);
	ras_store_bind(priv->stmt_extlog_record, extlog_event_fields, 4, ev->severity, -1);
	ras_store_bind(priv->stmt_extlog_record, extlog_event_fields, 5, ev->address, -1);
	ras_store_bind(priv->stmt_extlog_record, extlog_event_fields, 6, (uint64_t)ev->fru_id,  16);
	ras_store_bind(priv->stmt_extlog_record, extlog_event_fields, 7, (uint64_t)ev->fru_text, -1);
	ras_store_bind(priv->stmt_extlog_record, extlog_event_fields, 8, (uint64_t)ev->cper_data,  ev->cper_data_length);

	rc = sqlite3_step(priv->stmt_extlog_record);
	if (rc != SQLITE_DONE)
		log(TERM, LOG_ERR,
		    "Failed to do extlog_mem_record step on sqlite: error = %d\n", rc);
	rc = sqlite3_reset(priv->stmt_extlog_record);
	if (rc != SQLITE_OK)
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
	{ .name = "id",			.type = DB_TYPE_SERIAL, .is_pk = true },
	{ .name = "timestamp",		.type = DB_TYPE_TEXT },

	/* MCE registers */
	{ .name = "mcgcap",		.type = DB_TYPE_INT32 },
	{ .name = "mcgstatus",		.type = DB_TYPE_INT32 },
	{ .name = "status",		.type = DB_TYPE_INT64 },
	{ .name = "addr",		.type = DB_TYPE_INT64 }, // 5
	{ .name = "misc",		.type = DB_TYPE_INT64 },
	{ .name = "ip",			.type = DB_TYPE_INT64 },
	{ .name = "tsc",		.type = DB_TYPE_INT64 },
	{ .name = "walltime",		.type = DB_TYPE_INT64 },
	{ .name = "ppin",		.type = DB_TYPE_INT32 }, // 10
	{ .name = "cpu",		.type = DB_TYPE_INT32 },
	{ .name = "cpuid",		.type = DB_TYPE_INT32 },
	{ .name = "apicid",		.type = DB_TYPE_INT32 },
	{ .name = "socketid",		.type = DB_TYPE_INT32 },
	{ .name = "cs",			.type = DB_TYPE_INT32 }, // 15
	{ .name = "bank",		.type = DB_TYPE_INT32 },
	{ .name = "cpuvendor",		.type = DB_TYPE_INT32 },
	{ .name = "microcode",		.type = DB_TYPE_INT32 },

	/* Parsed data - will likely change */
	{ .name = "bank_name",		.type = DB_TYPE_TEXT },
	{ .name = "error_msg",		.type = DB_TYPE_TEXT }, // 20
	{ .name = "mcgstatus_msg",	.type = DB_TYPE_TEXT },
	{ .name = "mcistatus_msg",	.type = DB_TYPE_TEXT },
	{ .name = "mcastatus_msg",	.type = DB_TYPE_TEXT },
	{ .name = "user_action",	.type = DB_TYPE_TEXT },
	{ .name = "mc_location",	.type = DB_TYPE_TEXT },
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

	ras_store_bind(priv->stmt_mce_record, mce_record_fields, 1, (uint64_t)ev->timestamp, -1);
	ras_store_bind(priv->stmt_mce_record, mce_record_fields, 2, ev->mcgcap, -1);
	ras_store_bind(priv->stmt_mce_record, mce_record_fields, 3, ev->mcgstatus, -1);
	ras_store_bind(priv->stmt_mce_record, mce_record_fields, 4, ev->status, -1);
	ras_store_bind(priv->stmt_mce_record, mce_record_fields, 5, ev->addr, -1);
	ras_store_bind(priv->stmt_mce_record, mce_record_fields, 6, ev->misc, -1);
	ras_store_bind(priv->stmt_mce_record, mce_record_fields, 7, ev->ip, -1);
	ras_store_bind(priv->stmt_mce_record, mce_record_fields, 8, ev->tsc, -1);
	ras_store_bind(priv->stmt_mce_record, mce_record_fields, 9, ev->walltime, -1);
	ras_store_bind(priv->stmt_mce_record, mce_record_fields, 10, ev->ppin, -1);
	ras_store_bind(priv->stmt_mce_record, mce_record_fields, 11, ev->cpu, -1);
	ras_store_bind(priv->stmt_mce_record, mce_record_fields, 12, ev->cpuid, -1);
	ras_store_bind(priv->stmt_mce_record, mce_record_fields, 13, ev->apicid, -1);
	ras_store_bind(priv->stmt_mce_record, mce_record_fields, 14, ev->socketid, -1);
	ras_store_bind(priv->stmt_mce_record, mce_record_fields, 15, ev->cs, -1);
	ras_store_bind(priv->stmt_mce_record, mce_record_fields, 16, ev->bank, -1);
	ras_store_bind(priv->stmt_mce_record, mce_record_fields, 17, ev->cpuvendor, -1);
	ras_store_bind(priv->stmt_mce_record, mce_record_fields, 18, ev->microcode, -1);

	ras_store_bind(priv->stmt_mce_record, mce_record_fields, 19, (uint64_t)ev->bank_name, -1);
	ras_store_bind(priv->stmt_mce_record, mce_record_fields, 20, (uint64_t)ev->error_msg, -1);
	ras_store_bind(priv->stmt_mce_record, mce_record_fields, 21, (uint64_t)ev->mcgstatus_msg, -1);
	ras_store_bind(priv->stmt_mce_record, mce_record_fields, 22, (uint64_t)ev->mcistatus_msg, -1);
	ras_store_bind(priv->stmt_mce_record, mce_record_fields, 23, (uint64_t)ev->mcastatus_msg, -1);
	ras_store_bind(priv->stmt_mce_record, mce_record_fields, 24, (uint64_t)ev->user_action, -1);
	ras_store_bind(priv->stmt_mce_record, mce_record_fields, 25, (uint64_t)ev->mc_location, -1);

	rc = sqlite3_step(priv->stmt_mce_record);
	if (rc != SQLITE_DONE)
		log(TERM, LOG_ERR,
		    "Failed to do mce_record step on sqlite: error = %d\n", rc);
	rc = sqlite3_reset(priv->stmt_mce_record);
	if (rc != SQLITE_OK)
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
	{ .name = "id",			.type = DB_TYPE_SERIAL, .is_pk = true },
	{ .name = "timestamp",		.type = DB_TYPE_TEXT },
	{ .name = "bus_name",		.type = DB_TYPE_TEXT },
	{ .name = "dev_name",		.type = DB_TYPE_TEXT },
	{ .name = "driver_name",	.type = DB_TYPE_TEXT },
	{ .name = "reporter_name",	.type = DB_TYPE_TEXT },
	{ .name = "msg",		.type = DB_TYPE_TEXT },
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

	ras_store_bind(priv->stmt_devlink_event, devlink_event_fields, 1, (uint64_t)ev->timestamp, -1);
	ras_store_bind(priv->stmt_devlink_event, devlink_event_fields, 2, (uint64_t)ev->bus_name, -1);
	ras_store_bind(priv->stmt_devlink_event, devlink_event_fields, 3, (uint64_t)ev->dev_name, -1);
	ras_store_bind(priv->stmt_devlink_event, devlink_event_fields, 4, (uint64_t)ev->driver_name, -1);
	ras_store_bind(priv->stmt_devlink_event, devlink_event_fields, 5, (uint64_t)ev->reporter_name, -1);
	ras_store_bind(priv->stmt_devlink_event, devlink_event_fields, 6, (uint64_t)ev->msg, -1);

	rc = sqlite3_step(priv->stmt_devlink_event);
	if (rc != SQLITE_DONE)
		log(TERM, LOG_ERR,
		    "Failed to do devlink_event step on sqlite: error = %d\n", rc);
	rc = sqlite3_reset(priv->stmt_devlink_event);
	if (rc != SQLITE_OK)
		log(TERM, LOG_ERR,
		    "Failed reset devlink_event on sqlite: error = %d\n",
		    rc);
	log(TERM, LOG_INFO, "register inserted at db\n");

	return rc;
}
#endif

/*
 * Table and functions to handle block:block_rq_{complete|error}
 */

#ifdef HAVE_DISKERROR
static const struct db_fields diskerror_event_fields[] = {
	{ .name = "id",			.type = DB_TYPE_SERIAL, .is_pk = true },
	{ .name = "timestamp",		.type = DB_TYPE_TEXT },
	{ .name = "dev",		.type = DB_TYPE_TEXT },
	{ .name = "sector",		.type = DB_TYPE_INT64 },
	{ .name = "nr_sector",		.type = DB_TYPE_INT32 },
	{ .name = "error",		.type = DB_TYPE_TEXT },
	{ .name = "rwbs",		.type = DB_TYPE_TEXT },
	{ .name = "cmd",		.type = DB_TYPE_TEXT },
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
	log(TERM, LOG_INFO, "diskerror_event store: %p\n", priv->stmt_diskerror_event);

	ras_store_bind(priv->stmt_diskerror_event, diskerror_event_fields, 1, (uint64_t)ev->timestamp, -1);
	ras_store_bind(priv->stmt_diskerror_event, diskerror_event_fields, 2, (uint64_t)ev->dev, -1);
	ras_store_bind(priv->stmt_diskerror_event, diskerror_event_fields, 3, ev->sector, -1);
	ras_store_bind(priv->stmt_diskerror_event, diskerror_event_fields, 4, ev->nr_sector, -1);
	ras_store_bind(priv->stmt_diskerror_event, diskerror_event_fields, 5, (uint64_t)ev->error, -1);
	ras_store_bind(priv->stmt_diskerror_event, diskerror_event_fields, 6, (uint64_t)ev->rwbs, -1);
	ras_store_bind(priv->stmt_diskerror_event, diskerror_event_fields, 7, (uint64_t)ev->cmd, -1);

	rc = sqlite3_step(priv->stmt_diskerror_event);
	if (rc != SQLITE_DONE)
		log(TERM, LOG_ERR,
		    "Failed to do diskerror_event step on sqlite: error = %d\n", rc);
	rc = sqlite3_reset(priv->stmt_diskerror_event);
	if (rc != SQLITE_OK)
		log(TERM, LOG_ERR,
		    "Failed reset diskerror_event on sqlite: error = %d\n",
		    rc);
	log(TERM, LOG_INFO, "register inserted at db\n");

	return rc;
}
#endif

/*
 * Table and functions to handle ras:memory_failure
 */

#ifdef HAVE_MEMORY_FAILURE
static const struct db_fields mf_event_fields[] = {
	{ .name = "id",			.type = DB_TYPE_SERIAL, .is_pk = true },
	{ .name = "timestamp",		.type = DB_TYPE_TEXT },
	{ .name = "pfn",		.type = DB_TYPE_TEXT },
	{ .name = "page_type",		.type = DB_TYPE_TEXT },
	{ .name = "action_result",	.type = DB_TYPE_TEXT },
};

static const struct db_table_descriptor mf_event_tab = {
	.name = "memory_failure_event",
	.fields = mf_event_fields,
	.num_fields = ARRAY_SIZE(mf_event_fields),
};

int ras_store_mf_event(struct ras_events *ras, struct ras_mf_event *ev)
{
	int rc;
	struct sqlite3_priv *priv = ras->db_priv;

	if (!priv || !priv->stmt_mf_event)
		return 0;
	log(TERM, LOG_INFO, "memory_failure_event store: %p\n", priv->stmt_mf_event);

	ras_store_bind(priv->stmt_mf_event, mf_event_fields, 1, (uint64_t)ev->timestamp, -1);
	ras_store_bind(priv->stmt_mf_event, mf_event_fields, 2, (uint64_t)ev->pfn, -1);
	ras_store_bind(priv->stmt_mf_event, mf_event_fields, 3, (uint64_t)ev->page_type, -1);
	ras_store_bind(priv->stmt_mf_event, mf_event_fields, 4, (uint64_t)ev->action_result, -1);

	rc = sqlite3_step(priv->stmt_mf_event);
	if (rc != SQLITE_DONE)
		log(TERM, LOG_ERR,
		    "Failed to do memory_failure_event step on sqlite: error = %d\n", rc);

	rc = sqlite3_reset(priv->stmt_mf_event);
	if (rc != SQLITE_OK)
		log(TERM, LOG_ERR,
		    "Failed reset memory_failure_event on sqlite: error = %d\n",
		    rc);

	log(TERM, LOG_INFO, "register inserted at db\n");

	return rc;
}
#endif

#ifdef HAVE_CXL
/*
 * Table and functions to handle cxl:cxl_poison
 */
static const struct db_fields cxl_poison_event_fields[] = {
	{ .name = "id",			.type = DB_TYPE_SERIAL, .is_pk = true },
	{ .name = "timestamp",		.type = DB_TYPE_TEXT },
	{ .name = "memdev",		.type = DB_TYPE_TEXT },
	{ .name = "host",		.type = DB_TYPE_TEXT },
	{ .name = "serial",		.type = DB_TYPE_INT64 },
	{ .name = "trace_type",		.type = DB_TYPE_TEXT },
	{ .name = "region",		.type = DB_TYPE_TEXT },
	{ .name = "region_uuid",	.type = DB_TYPE_TEXT },
	{ .name = "hpa",		.type = DB_TYPE_INT64 },
	{ .name = "dpa",		.type = DB_TYPE_INT64 },
	{ .name = "dpa_length",		.type = DB_TYPE_INT32 },
	{ .name = "source",		.type = DB_TYPE_TEXT },
	{ .name = "flags",		.type = DB_TYPE_INT32 },
	{ .name = "overflow_ts",	.type = DB_TYPE_TEXT },
	{ .name = "hpa_alias0",		.type = DB_TYPE_INT64},
};

static const struct db_table_descriptor cxl_poison_event_tab = {
	.name = "cxl_poison_event",
	.fields = cxl_poison_event_fields,
	.num_fields = ARRAY_SIZE(cxl_poison_event_fields),
};

int ras_store_cxl_poison_event(struct ras_events *ras, struct ras_cxl_poison_event *ev)
{
	int rc;
	struct sqlite3_priv *priv = ras->db_priv;

	if (!priv || !priv->stmt_cxl_poison_event)
		return 0;
	log(TERM, LOG_INFO, "cxl_poison_event store: %p\n", priv->stmt_cxl_poison_event);

	ras_store_bind(priv->stmt_cxl_poison_event, cxl_poison_event_fields, 1, (uint64_t)ev->timestamp, -1);
	ras_store_bind(priv->stmt_cxl_poison_event, cxl_poison_event_fields, 2, (uint64_t)ev->memdev, -1);
	ras_store_bind(priv->stmt_cxl_poison_event, cxl_poison_event_fields, 3, (uint64_t)ev->host, -1);
	ras_store_bind(priv->stmt_cxl_poison_event, cxl_poison_event_fields, 4, ev->serial, -1);
	ras_store_bind(priv->stmt_cxl_poison_event, cxl_poison_event_fields, 5, (uint64_t)ev->trace_type, -1);
	ras_store_bind(priv->stmt_cxl_poison_event, cxl_poison_event_fields, 6, (uint64_t)ev->region, -1);
	ras_store_bind(priv->stmt_cxl_poison_event, cxl_poison_event_fields, 7, (uint64_t)ev->uuid, -1);
	ras_store_bind(priv->stmt_cxl_poison_event, cxl_poison_event_fields, 8, ev->hpa, -1);
	ras_store_bind(priv->stmt_cxl_poison_event, cxl_poison_event_fields, 9, ev->dpa, -1);
	ras_store_bind(priv->stmt_cxl_poison_event, cxl_poison_event_fields, 10, ev->dpa_length, -1);
	ras_store_bind(priv->stmt_cxl_poison_event, cxl_poison_event_fields, 11, (uint64_t)ev->source, -1);
	ras_store_bind(priv->stmt_cxl_poison_event, cxl_poison_event_fields, 12, ev->flags, -1);
	ras_store_bind(priv->stmt_cxl_poison_event, cxl_poison_event_fields, 13, (uint64_t)ev->overflow_ts, -1);
	ras_store_bind(priv->stmt_cxl_poison_event, cxl_poison_event_fields, 14, ev->hpa_alias0, -1);

	rc = sqlite3_step(priv->stmt_cxl_poison_event);
	if (rc != SQLITE_DONE)
		log(TERM, LOG_ERR,
		    "Failed to do cxl_poison_event step on sqlite: error = %d\n", rc);
	rc = sqlite3_reset(priv->stmt_cxl_poison_event);
	if (rc != SQLITE_OK)
		log(TERM, LOG_ERR,
		    "Failed reset cxl_poison_event on sqlite: error = %d\n",
		    rc);
	log(TERM, LOG_INFO, "register inserted at db\n");

	return rc;
}

/*
 * Table and functions to handle cxl:cxl_aer_uncorrectable_error
 */
static const struct db_fields cxl_aer_ue_event_fields[] = {
	{ .name = "id",			.type = DB_TYPE_SERIAL, .is_pk = true },
	{ .name = "timestamp",		.type = DB_TYPE_TEXT },
	{ .name = "memdev",		.type = DB_TYPE_TEXT },
	{ .name = "host",		.type = DB_TYPE_TEXT },
	{ .name = "serial",		.type = DB_TYPE_INT64 },
	{ .name = "error_status",	.type = DB_TYPE_INT32 },
	{ .name = "first_error",	.type = DB_TYPE_INT32 },
	{ .name = "header_log",		.type = DB_TYPE_BLOB },
};

static const struct db_table_descriptor cxl_aer_ue_event_tab = {
	.name = "cxl_aer_ue_event",
	.fields = cxl_aer_ue_event_fields,
	.num_fields = ARRAY_SIZE(cxl_aer_ue_event_fields),
};

int ras_store_cxl_aer_ue_event(struct ras_events *ras, struct ras_cxl_aer_ue_event *ev)
{
	int rc;
	struct sqlite3_priv *priv = ras->db_priv;

	if (!priv || !priv->stmt_cxl_aer_ue_event)
		return 0;
	log(TERM, LOG_INFO, "cxl_aer_ue_event store: %p\n", priv->stmt_cxl_aer_ue_event);

	ras_store_bind(priv->stmt_cxl_aer_ue_event, cxl_aer_ue_event_fields, 1, (uint64_t)ev->timestamp, -1);
	ras_store_bind(priv->stmt_cxl_aer_ue_event, cxl_aer_ue_event_fields, 2, (uint64_t)ev->memdev, -1);
	ras_store_bind(priv->stmt_cxl_aer_ue_event, cxl_aer_ue_event_fields, 3, (uint64_t)ev->host, -1);
	ras_store_bind(priv->stmt_cxl_aer_ue_event, cxl_aer_ue_event_fields, 4, ev->serial, -1);
	ras_store_bind(priv->stmt_cxl_aer_ue_event, cxl_aer_ue_event_fields, 5, ev->error_status, -1);
	ras_store_bind(priv->stmt_cxl_aer_ue_event, cxl_aer_ue_event_fields, 6, ev->first_error, -1);
	ras_store_bind(priv->stmt_cxl_aer_ue_event, cxl_aer_ue_event_fields, 7, (uint64_t)ev->header_log, CXL_HEADERLOG_SIZE);

	rc = sqlite3_step(priv->stmt_cxl_aer_ue_event);
	if (rc != SQLITE_DONE)
		log(TERM, LOG_ERR,
		    "Failed to do cxl_aer_ue_event step on sqlite: error = %d\n", rc);
	rc = sqlite3_reset(priv->stmt_cxl_aer_ue_event);
	if (rc != SQLITE_OK)
		log(TERM, LOG_ERR,
		    "Failed reset cxl_aer_ue_event on sqlite: error = %d\n",
		    rc);
	log(TERM, LOG_INFO, "register inserted at db\n");

	return rc;
}

/*
 * Table and functions to handle cxl:cxl_aer_correctable_error
 */
static const struct db_fields cxl_aer_ce_event_fields[] = {
	{ .name = "id",			.type = DB_TYPE_SERIAL, .is_pk = true },
	{ .name = "timestamp",		.type = DB_TYPE_TEXT },
	{ .name = "memdev",		.type = DB_TYPE_TEXT },
	{ .name = "host",		.type = DB_TYPE_TEXT },
	{ .name = "serial",		.type = DB_TYPE_INT64 },
	{ .name = "error_status",	.type = DB_TYPE_INT32 },
};

static const struct db_table_descriptor cxl_aer_ce_event_tab = {
	.name = "cxl_aer_ce_event",
	.fields = cxl_aer_ce_event_fields,
	.num_fields = ARRAY_SIZE(cxl_aer_ce_event_fields),
};

int ras_store_cxl_aer_ce_event(struct ras_events *ras, struct ras_cxl_aer_ce_event *ev)
{
	int rc;
	struct sqlite3_priv *priv = ras->db_priv;

	if (!priv || !priv->stmt_cxl_aer_ce_event)
		return 0;
	log(TERM, LOG_INFO, "cxl_aer_ce_event store: %p\n", priv->stmt_cxl_aer_ce_event);

	ras_store_bind(priv->stmt_cxl_aer_ce_event, cxl_aer_ce_event_fields, 1, (uint64_t)ev->timestamp, -1);
	ras_store_bind(priv->stmt_cxl_aer_ce_event, cxl_aer_ce_event_fields, 2, (uint64_t)ev->memdev, -1);
	ras_store_bind(priv->stmt_cxl_aer_ce_event, cxl_aer_ce_event_fields, 3, (uint64_t)ev->host, -1);
	ras_store_bind(priv->stmt_cxl_aer_ce_event, cxl_aer_ce_event_fields, 4, ev->serial, -1);
	ras_store_bind(priv->stmt_cxl_aer_ce_event, cxl_aer_ce_event_fields, 5, ev->error_status, -1);

	rc = sqlite3_step(priv->stmt_cxl_aer_ce_event);
	if (rc != SQLITE_DONE)
		log(TERM, LOG_ERR,
		    "Failed to do cxl_aer_ce_event step on sqlite: error = %d\n", rc);
	rc = sqlite3_reset(priv->stmt_cxl_aer_ce_event);
	if (rc != SQLITE_OK)
		log(TERM, LOG_ERR,
		    "Failed reset cxl_aer_ce_event on sqlite: error = %d\n",
		    rc);
	log(TERM, LOG_INFO, "register inserted at db\n");

	return rc;
}

/*
 * Table and functions to handle cxl:cxl_overflow
 */
static const struct db_fields cxl_overflow_event_fields[] = {
	{ .name = "id",			.type = DB_TYPE_SERIAL, .is_pk = true },
	{ .name = "timestamp",		.type = DB_TYPE_TEXT },
	{ .name = "memdev",		.type = DB_TYPE_TEXT },
	{ .name = "host",		.type = DB_TYPE_TEXT },
	{ .name = "serial",		.type = DB_TYPE_INT64 },
	{ .name = "log_type",		.type = DB_TYPE_TEXT },
	{ .name = "count",		.type = DB_TYPE_INT32 },
	{ .name = "first_ts",		.type = DB_TYPE_TEXT },
	{ .name = "last_ts",		.type = DB_TYPE_TEXT },
};

static const struct db_table_descriptor cxl_overflow_event_tab = {
	.name = "cxl_overflow_event",
	.fields = cxl_overflow_event_fields,
	.num_fields = ARRAY_SIZE(cxl_overflow_event_fields),
};

int ras_store_cxl_overflow_event(struct ras_events *ras, struct ras_cxl_overflow_event *ev)
{
	int rc;
	struct sqlite3_priv *priv = ras->db_priv;

	if (!priv || !priv->stmt_cxl_overflow_event)
		return 0;
	log(TERM, LOG_INFO, "cxl_overflow_event store: %p\n", priv->stmt_cxl_overflow_event);

	ras_store_bind(priv->stmt_cxl_overflow_event, cxl_overflow_event_fields, 1, (uint64_t)ev->timestamp, -1);
	ras_store_bind(priv->stmt_cxl_overflow_event, cxl_overflow_event_fields, 2, (uint64_t)ev->memdev, -1);
	ras_store_bind(priv->stmt_cxl_overflow_event, cxl_overflow_event_fields, 3, (uint64_t)ev->host, -1);
	ras_store_bind(priv->stmt_cxl_overflow_event, cxl_overflow_event_fields, 4, ev->serial, -1);
	ras_store_bind(priv->stmt_cxl_overflow_event, cxl_overflow_event_fields, 5, (uint64_t)ev->log_type, -1);
	ras_store_bind(priv->stmt_cxl_overflow_event, cxl_overflow_event_fields, 6, ev->count, -1);
	ras_store_bind(priv->stmt_cxl_overflow_event, cxl_overflow_event_fields, 7, (uint64_t)ev->first_ts, -1);
	ras_store_bind(priv->stmt_cxl_overflow_event, cxl_overflow_event_fields, 8, (uint64_t)ev->last_ts, -1);

	rc = sqlite3_step(priv->stmt_cxl_overflow_event);
	if (rc != SQLITE_DONE)
		log(TERM, LOG_ERR,
		    "Failed to do cxl_overflow_event step on sqlite: error = %d\n", rc);
	rc = sqlite3_reset(priv->stmt_cxl_overflow_event);
	if (rc != SQLITE_OK)
		log(TERM, LOG_ERR,
		    "Failed reset cxl_overflow_event on sqlite: error = %d\n",
		    rc);
	log(TERM, LOG_INFO, "register inserted at db\n");

	return rc;
}

static int ras_store_cxl_common_hdr(sqlite3_stmt *stmt,
				    const struct db_fields *fields,
				    struct ras_cxl_event_common_hdr *hdr)
{
	int idx = 1;

	if (!stmt || !hdr)
		return -1;

	ras_store_bind(stmt, fields, idx++, (uint64_t)hdr->timestamp, -1);
	ras_store_bind(stmt, fields, idx++, (uint64_t)hdr->memdev, -1);
	ras_store_bind(stmt, fields, idx++, (uint64_t)hdr->host, -1);
	ras_store_bind(stmt, fields, idx++, hdr->serial, -1);
	ras_store_bind(stmt, fields, idx++, (uint64_t)hdr->log_type, -1);
	ras_store_bind(stmt, fields, idx++, (uint64_t)hdr->hdr_uuid, -1);
	ras_store_bind(stmt, fields, idx++, hdr->hdr_flags, -1);
	ras_store_bind(stmt, fields, idx++, hdr->hdr_handle, -1);
	ras_store_bind(stmt, fields, idx++, hdr->hdr_related_handle, -1);
	ras_store_bind(stmt, fields, idx++, (uint64_t)hdr->hdr_timestamp, -1);
	ras_store_bind(stmt, fields, idx++, hdr->hdr_length, -1);
	ras_store_bind(stmt, fields, idx++, hdr->hdr_maint_op_class, -1);
	ras_store_bind(stmt, fields, idx++, hdr->hdr_maint_op_sub_class, -1);
	ras_store_bind(stmt, fields, idx++, hdr->hdr_ld_id, -1);
	ras_store_bind(stmt, fields, idx++, hdr->hdr_head_id, -1);

	return idx;
}

/*
 * Table and functions to handle cxl:cxl_generic_event
 */
static const struct db_fields cxl_generic_event_fields[] = {
	{ .name = "id",				.type = DB_TYPE_SERIAL, .is_pk = true },
	{ .name = "timestamp",			.type = DB_TYPE_TEXT },
	{ .name = "memdev",			.type = DB_TYPE_TEXT },
	{ .name = "host",			.type = DB_TYPE_TEXT },
	{ .name = "serial",			.type = DB_TYPE_INT64 },
	{ .name = "log_type",			.type = DB_TYPE_TEXT },
	{ .name = "hdr_uuid",			.type = DB_TYPE_TEXT },
	{ .name = "hdr_flags",			.type = DB_TYPE_INT32 },
	{ .name = "hdr_handle",			.type = DB_TYPE_INT32 },
	{ .name = "hdr_related_handle",		.type = DB_TYPE_INT32 },
	{ .name = "hdr_ts",			.type = DB_TYPE_TEXT },
	{ .name = "hdr_length",			.type = DB_TYPE_INT32 },
	{ .name = "hdr_maint_op_class",		.type = DB_TYPE_INT32 },
	{ .name = "hdr_maint_op_sub_class",	.type = DB_TYPE_INT32 },
	{ .name = "hdr_ld_id",			.type = DB_TYPE_INT32 },
	{ .name = "hdr_head_id",		.type = DB_TYPE_INT32 },
	{ .name = "data",			.type = DB_TYPE_BLOB },
};

static const struct db_table_descriptor cxl_generic_event_tab = {
	.name = "cxl_generic_event",
	.fields = cxl_generic_event_fields,
	.num_fields = ARRAY_SIZE(cxl_generic_event_fields),
};

int ras_store_cxl_generic_event(struct ras_events *ras, struct ras_cxl_generic_event *ev)
{
	struct sqlite3_priv *priv = ras->db_priv;
	int idx;
	int rc;

	if (!priv || !priv->stmt_cxl_generic_event)
		return -1;
	log(TERM, LOG_INFO, "cxl_generic_event store: %p\n", priv->stmt_cxl_generic_event);

	idx = ras_store_cxl_common_hdr(priv->stmt_cxl_generic_event,
				       cxl_generic_event_fields,
				       &ev->hdr);
	if (idx <= 0)
		return -1;

	ras_store_bind(priv->stmt_cxl_generic_event, cxl_generic_event_fields,
		       idx++, (uint64_t)ev->data, CXL_EVENT_RECORD_DATA_LENGTH);

	rc = sqlite3_step(priv->stmt_cxl_generic_event);
	if (rc != SQLITE_DONE)
		log(TERM, LOG_ERR,
		    "Failed to do stmt_cxl_generic_event step on sqlite: error = %d\n", rc);
	rc = sqlite3_reset(priv->stmt_cxl_generic_event);
	if (rc != SQLITE_OK)
		log(TERM, LOG_ERR,
		    "Failed reset stmt_cxl_generic_event on sqlite: error = %d\n", rc);
	log(TERM, LOG_INFO, "register inserted at db\n");

	return rc;
}

/*
 * Table and functions to handle cxl:cxl_general_media_event
 */
static const struct db_fields cxl_general_media_event_fields[] = {
	{ .name = "id",				.type = DB_TYPE_SERIAL, .is_pk = true },
	{ .name = "timestamp",			.type = DB_TYPE_TEXT },
	{ .name = "memdev",			.type = DB_TYPE_TEXT },
	{ .name = "host",			.type = DB_TYPE_TEXT },
	{ .name = "serial",			.type = DB_TYPE_INT64 },
	{ .name = "log_type",			.type = DB_TYPE_TEXT },
	{ .name = "hdr_uuid",			.type = DB_TYPE_TEXT },
	{ .name = "hdr_flags",			.type = DB_TYPE_INT32 },
	{ .name = "hdr_handle",			.type = DB_TYPE_INT32 },
	{ .name = "hdr_related_handle",		.type = DB_TYPE_INT32 },
	{ .name = "hdr_ts",			.type = DB_TYPE_TEXT },
	{ .name = "hdr_length",			.type = DB_TYPE_INT32 },
	{ .name = "hdr_maint_op_class",		.type = DB_TYPE_INT32 },
	{ .name = "hdr_maint_op_sub_class",	.type = DB_TYPE_INT32 },
	{ .name = "hdr_ld_id",			.type = DB_TYPE_INT32 },
	{ .name = "hdr_head_id",		.type = DB_TYPE_INT32 },

	{ .name = "dpa",			.type = DB_TYPE_INT64 },
	{ .name = "dpa_flags",			.type = DB_TYPE_INT32 },
	{ .name = "descriptor",			.type = DB_TYPE_INT32 },
	{ .name = "type",			.type = DB_TYPE_INT32 },
	{ .name = "transaction_type",		.type = DB_TYPE_INT32 },
	{ .name = "channel",			.type = DB_TYPE_INT32 },
	{ .name = "rank",			.type = DB_TYPE_INT32 },
	{ .name = "device",			.type = DB_TYPE_INT32 },
	{ .name = "comp_id",			.type = DB_TYPE_BLOB },
	{ .name = "hpa",			.type = DB_TYPE_INT64 },
	{ .name = "region",			.type = DB_TYPE_TEXT },
	{ .name = "region_uuid",		.type = DB_TYPE_TEXT },
	{ .name = "pldm_entity_id",		.type = DB_TYPE_BLOB },
	{ .name = "pldm_resource_id",		.type = DB_TYPE_BLOB },
	{ .name = "sub_type",			.type = DB_TYPE_INT32 },
	{ .name = "cme_threshold_ev_flags",	.type = DB_TYPE_INT32 },
	{ .name = "cme_count",			.type = DB_TYPE_INT32 },
	{ .name = "hpa_alias0",			.type = DB_TYPE_INT64 },
};

static const struct db_table_descriptor cxl_general_media_event_tab = {
	.name = "cxl_general_media_event",
	.fields = cxl_general_media_event_fields,
	.num_fields = ARRAY_SIZE(cxl_general_media_event_fields),
};

int ras_store_cxl_general_media_event(struct ras_events *ras,
				      struct ras_cxl_general_media_event *ev)
{
	struct sqlite3_priv *priv = ras->db_priv;
	int idx;
	int rc;

	if (!priv || !priv->stmt_cxl_general_media_event)
		return -1;
	log(TERM, LOG_INFO, "cxl_general_media_event store: %p\n",
	    priv->stmt_cxl_general_media_event);

	idx = ras_store_cxl_common_hdr(priv->stmt_cxl_general_media_event,
				       cxl_general_media_event_fields,
				       &ev->hdr);
	if (idx <= 0)
		return -1;
	ras_store_bind(priv->stmt_cxl_general_media_event, cxl_general_media_event_fields, idx++, ev->dpa, -1);
	ras_store_bind(priv->stmt_cxl_general_media_event, cxl_general_media_event_fields, idx++, ev->dpa_flags, -1);
	ras_store_bind(priv->stmt_cxl_general_media_event, cxl_general_media_event_fields, idx++, ev->descriptor, -1);
	ras_store_bind(priv->stmt_cxl_general_media_event, cxl_general_media_event_fields, idx++, ev->type, -1);
	ras_store_bind(priv->stmt_cxl_general_media_event, cxl_general_media_event_fields, idx++, ev->transaction_type, -1);
	ras_store_bind(priv->stmt_cxl_general_media_event, cxl_general_media_event_fields, idx++, ev->channel, -1);
	ras_store_bind(priv->stmt_cxl_general_media_event, cxl_general_media_event_fields, idx++, ev->rank, -1);
	ras_store_bind(priv->stmt_cxl_general_media_event, cxl_general_media_event_fields, idx++, ev->device, -1);
	ras_store_bind(priv->stmt_cxl_general_media_event, cxl_general_media_event_fields, idx++, (uint64_t)ev->comp_id, CXL_EVENT_GEN_MED_COMP_ID_SIZE);
	ras_store_bind(priv->stmt_cxl_general_media_event, cxl_general_media_event_fields, idx++, ev->hpa, -1);
	ras_store_bind(priv->stmt_cxl_general_media_event, cxl_general_media_event_fields, idx++, (uint64_t)ev->region, -1);
	ras_store_bind(priv->stmt_cxl_general_media_event, cxl_general_media_event_fields, idx++, (uint64_t)ev->region_uuid, -1);
	ras_store_bind(priv->stmt_cxl_general_media_event, cxl_general_media_event_fields, idx++, (uint64_t)ev->entity_id, CXL_PLDM_ENTITY_ID_LEN);
	ras_store_bind(priv->stmt_cxl_general_media_event, cxl_general_media_event_fields, idx++, (uint64_t)ev->res_id, CXL_PLDM_RES_ID_LEN);
	ras_store_bind(priv->stmt_cxl_general_media_event, cxl_general_media_event_fields, idx++, ev->sub_type, -1);
	ras_store_bind(priv->stmt_cxl_general_media_event, cxl_general_media_event_fields, idx++, ev->cme_threshold_ev_flags, -1);
	ras_store_bind(priv->stmt_cxl_general_media_event, cxl_general_media_event_fields, idx++, ev->cme_count, -1);
	ras_store_bind(priv->stmt_cxl_general_media_event, cxl_general_media_event_fields, idx++, ev->hpa_alias0, -1);

	rc = sqlite3_step(priv->stmt_cxl_general_media_event);
	if (rc != SQLITE_DONE)
		log(TERM, LOG_ERR,
		    "Failed to do stmt_cxl_general_media_event step on sqlite: error = %d\n", rc);
	rc = sqlite3_reset(priv->stmt_cxl_general_media_event);
	if (rc != SQLITE_OK)
		log(TERM, LOG_ERR,
		    "Failed reset stmt_cxl_general_media_event on sqlite: error = %d\n", rc);
	log(TERM, LOG_INFO, "register inserted at db\n");

	return rc;
}

/*
 * Table and functions to handle cxl:cxl_dram_event
 */
static const struct db_fields cxl_dram_event_fields[] = {
	{ .name = "id",				.type = DB_TYPE_SERIAL, .is_pk = true },
	{ .name = "timestamp",			.type = DB_TYPE_TEXT },
	{ .name = "memdev",			.type = DB_TYPE_TEXT },
	{ .name = "host",			.type = DB_TYPE_TEXT },
	{ .name = "serial",			.type = DB_TYPE_INT64 },
	{ .name = "log_type",			.type = DB_TYPE_TEXT },
	{ .name = "hdr_uuid",			.type = DB_TYPE_TEXT },
	{ .name = "hdr_flags",			.type = DB_TYPE_INT32 },
	{ .name = "hdr_handle",			.type = DB_TYPE_INT32 },
	{ .name = "hdr_related_handle",		.type = DB_TYPE_INT32 },
	{ .name = "hdr_ts",			.type = DB_TYPE_TEXT },
	{ .name = "hdr_length",			.type = DB_TYPE_INT32 },
	{ .name = "hdr_maint_op_class",		.type = DB_TYPE_INT32 },
	{ .name = "hdr_maint_op_sub_class",	.type = DB_TYPE_INT32 },
	{ .name = "hdr_ld_id",			.type = DB_TYPE_INT32 },
	{ .name = "hdr_head_id",		.type = DB_TYPE_INT32 },

	{ .name = "dpa",			.type = DB_TYPE_INT64 },
	{ .name = "dpa_flags",			.type = DB_TYPE_INT32 },
	{ .name = "descriptor",			.type = DB_TYPE_INT32 },
	{ .name = "type",			.type = DB_TYPE_INT32 },
	{ .name = "transaction_type",		.type = DB_TYPE_INT32 },
	{ .name = "channel",			.type = DB_TYPE_INT32 },
	{ .name = "rank",			.type = DB_TYPE_INT32 },
	{ .name = "nibble_mask",		.type = DB_TYPE_INT32 },
	{ .name = "bank_group",			.type = DB_TYPE_INT32 },
	{ .name = "bank",			.type = DB_TYPE_INT32 },
	{ .name = "row",			.type = DB_TYPE_INT32 },
	{ .name = "column",			.type = DB_TYPE_INT32 },
	{ .name = "cor_mask",			.type = DB_TYPE_BLOB },
	{ .name = "hpa",			.type = DB_TYPE_INT64 },
	{ .name = "region",			.type = DB_TYPE_TEXT },
	{ .name = "region_uuid",		.type = DB_TYPE_TEXT },
	{ .name = "comp_id",			.type = DB_TYPE_BLOB },
	{ .name = "pldm_entity_id",		.type = DB_TYPE_BLOB },
	{ .name = "pldm_resource_id",		.type = DB_TYPE_BLOB },
	{ .name = "sub_type",			.type = DB_TYPE_INT32 },
	{ .name = "sub_channel",		.type = DB_TYPE_INT32 },
	{ .name = "cme_threshold_ev_flags",	.type = DB_TYPE_INT32 },
	{ .name = "cvme_count",			.type = DB_TYPE_INT32 },
	{ .name = "hpa_alias0",			.type = DB_TYPE_INT64 },
};

static const struct db_table_descriptor cxl_dram_event_tab = {
	.name = "cxl_dram_event",
	.fields = cxl_dram_event_fields,
	.num_fields = ARRAY_SIZE(cxl_dram_event_fields),
};

int ras_store_cxl_dram_event(struct ras_events *ras, struct ras_cxl_dram_event *ev)
{
	struct sqlite3_priv *priv = ras->db_priv;
	int idx;
	int rc;

	if (!priv || !priv->stmt_cxl_dram_event)
		return -1;
	log(TERM, LOG_INFO, "cxl_dram_event store: %p\n",
	    priv->stmt_cxl_dram_event);

	idx = ras_store_cxl_common_hdr(priv->stmt_cxl_dram_event,
				       cxl_dram_event_fields,
				       &ev->hdr);
	if (idx <= 0)
		return -1;
	ras_store_bind(priv->stmt_cxl_dram_event, cxl_dram_event_fields, idx++, ev->dpa, -1);
	ras_store_bind(priv->stmt_cxl_dram_event, cxl_dram_event_fields, idx++, ev->dpa_flags, -1);
	ras_store_bind(priv->stmt_cxl_dram_event, cxl_dram_event_fields, idx++, ev->descriptor, -1);
	ras_store_bind(priv->stmt_cxl_dram_event, cxl_dram_event_fields, idx++, ev->type, -1);
	ras_store_bind(priv->stmt_cxl_dram_event, cxl_dram_event_fields, idx++, ev->transaction_type, -1);
	ras_store_bind(priv->stmt_cxl_dram_event, cxl_dram_event_fields, idx++, ev->channel, -1);
	ras_store_bind(priv->stmt_cxl_dram_event, cxl_dram_event_fields, idx++, ev->rank, -1);
	ras_store_bind(priv->stmt_cxl_dram_event, cxl_dram_event_fields, idx++, ev->nibble_mask, -1);
	ras_store_bind(priv->stmt_cxl_dram_event, cxl_dram_event_fields, idx++, ev->bank_group, -1);
	ras_store_bind(priv->stmt_cxl_dram_event, cxl_dram_event_fields, idx++, ev->bank, -1);
	ras_store_bind(priv->stmt_cxl_dram_event, cxl_dram_event_fields, idx++, ev->row, -1);
	ras_store_bind(priv->stmt_cxl_dram_event, cxl_dram_event_fields, idx++, ev->column, -1);
	ras_store_bind(priv->stmt_cxl_dram_event, cxl_dram_event_fields, idx++, (uint64_t)ev->cor_mask, CXL_EVENT_DER_CORRECTION_MASK_SIZE);
	ras_store_bind(priv->stmt_cxl_dram_event, cxl_dram_event_fields, idx++, ev->hpa, -1);
	ras_store_bind(priv->stmt_cxl_dram_event, cxl_dram_event_fields, idx++, (uint64_t)ev->region, -1);
	ras_store_bind(priv->stmt_cxl_dram_event, cxl_dram_event_fields, idx++, (uint64_t)ev->region_uuid, -1);
	ras_store_bind(priv->stmt_cxl_dram_event, cxl_dram_event_fields, idx++, (uint64_t)ev->comp_id, CXL_EVENT_GEN_MED_COMP_ID_SIZE);
	ras_store_bind(priv->stmt_cxl_dram_event, cxl_dram_event_fields, idx++, (uint64_t)ev->entity_id, CXL_PLDM_ENTITY_ID_LEN);
	ras_store_bind(priv->stmt_cxl_dram_event, cxl_dram_event_fields, idx++, (uint64_t)ev->res_id, CXL_PLDM_RES_ID_LEN);
	ras_store_bind(priv->stmt_cxl_dram_event, cxl_dram_event_fields, idx++, ev->sub_type, -1);
	ras_store_bind(priv->stmt_cxl_dram_event, cxl_dram_event_fields, idx++, ev->sub_channel, -1);
	ras_store_bind(priv->stmt_cxl_dram_event, cxl_dram_event_fields, idx++, ev->cme_threshold_ev_flags, -1);
	ras_store_bind(priv->stmt_cxl_dram_event, cxl_dram_event_fields, idx++, ev->cvme_count, -1);
	ras_store_bind(priv->stmt_cxl_dram_event, cxl_dram_event_fields, idx++, ev->hpa_alias0, -1);

	rc = sqlite3_step(priv->stmt_cxl_dram_event);
	if (rc != SQLITE_DONE)
		log(TERM, LOG_ERR,
		    "Failed to do stmt_cxl_dram_event step on sqlite: error = %d\n", rc);
	rc = sqlite3_reset(priv->stmt_cxl_dram_event);
	if (rc != SQLITE_OK)
		log(TERM, LOG_ERR,
		    "Failed reset stmt_cxl_dram_event on sqlite: error = %d\n", rc);
	log(TERM, LOG_INFO, "register inserted at db\n");

	return rc;
}

/*
 * Table and functions to handle cxl:cxl_memory_module_event
 */
static const struct db_fields cxl_memory_module_event_fields[] = {
	{ .name = "id",				.type = DB_TYPE_SERIAL, .is_pk = true },
	{ .name = "timestamp",			.type = DB_TYPE_TEXT },
	{ .name = "memdev",			.type = DB_TYPE_TEXT },
	{ .name = "host",			.type = DB_TYPE_TEXT },
	{ .name = "serial",			.type = DB_TYPE_INT64 },
	{ .name = "log_type",			.type = DB_TYPE_TEXT },
	{ .name = "hdr_uuid",			.type = DB_TYPE_TEXT },
	{ .name = "hdr_flags",			.type = DB_TYPE_INT32 },
	{ .name = "hdr_handle",			.type = DB_TYPE_INT32 },
	{ .name = "hdr_related_handle",		.type = DB_TYPE_INT32 },
	{ .name = "hdr_ts",			.type = DB_TYPE_TEXT },
	{ .name = "hdr_length",			.type = DB_TYPE_INT32 },
	{ .name = "hdr_maint_op_class",		.type = DB_TYPE_INT32 },
	{ .name = "hdr_maint_op_sub_class",	.type = DB_TYPE_INT32 },
	{ .name = "hdr_ld_id",			.type = DB_TYPE_INT32 },
	{ .name = "hdr_head_id",		.type = DB_TYPE_INT32 },

	{ .name = "event_type",			.type = DB_TYPE_INT32 },
	{ .name = "health_status",		.type = DB_TYPE_INT32 },
	{ .name = "media_status",		.type = DB_TYPE_INT32 },
	{ .name = "life_used",			.type = DB_TYPE_INT32 },
	{ .name = "dirty_shutdown_cnt",		.type = DB_TYPE_INT32 },
	{ .name = "cor_vol_err_cnt",		.type = DB_TYPE_INT32 },
	{ .name = "cor_per_err_cnt",		.type = DB_TYPE_INT32 },
	{ .name = "device_temp",		.type = DB_TYPE_INT32 },
	{ .name = "add_status",			.type = DB_TYPE_INT32 },
	{ .name = "event_sub_type",		.type = DB_TYPE_INT32 },
	{ .name = "comp_id",			.type = DB_TYPE_BLOB },
	{ .name = "pldm_entity_id",		.type = DB_TYPE_BLOB },
	{ .name = "pldm_resource_id",		.type = DB_TYPE_BLOB },
};

static const struct db_table_descriptor cxl_memory_module_event_tab = {
	.name = "cxl_memory_module_event",
	.fields = cxl_memory_module_event_fields,
	.num_fields = ARRAY_SIZE(cxl_memory_module_event_fields),
};

int ras_store_cxl_memory_module_event(struct ras_events *ras,
				      struct ras_cxl_memory_module_event *ev)
{
	struct sqlite3_priv *priv = ras->db_priv;
	int idx;
	int rc;

	if (!priv || !priv->stmt_cxl_memory_module_event)
		return -1;
	log(TERM, LOG_INFO, "cxl_memory_module_event store: %p\n",
	    priv->stmt_cxl_memory_module_event);

	idx = ras_store_cxl_common_hdr(priv->stmt_cxl_memory_module_event,
				       cxl_memory_module_event_fields,
				       &ev->hdr);
	if (idx <= 0)
		return -1;
	ras_store_bind(priv->stmt_cxl_memory_module_event, cxl_memory_module_event_fields, idx++, ev->event_type, -1);
	ras_store_bind(priv->stmt_cxl_memory_module_event, cxl_memory_module_event_fields, idx++, ev->health_status, -1);
	ras_store_bind(priv->stmt_cxl_memory_module_event, cxl_memory_module_event_fields, idx++, ev->media_status, -1);
	ras_store_bind(priv->stmt_cxl_memory_module_event, cxl_memory_module_event_fields, idx++, ev->life_used, -1);
	ras_store_bind(priv->stmt_cxl_memory_module_event, cxl_memory_module_event_fields, idx++, ev->dirty_shutdown_cnt, -1);
	ras_store_bind(priv->stmt_cxl_memory_module_event, cxl_memory_module_event_fields, idx++, ev->cor_vol_err_cnt, -1);
	ras_store_bind(priv->stmt_cxl_memory_module_event, cxl_memory_module_event_fields, idx++, ev->cor_per_err_cnt, -1);
	ras_store_bind(priv->stmt_cxl_memory_module_event, cxl_memory_module_event_fields, idx++, ev->device_temp, -1);
	ras_store_bind(priv->stmt_cxl_memory_module_event, cxl_memory_module_event_fields, idx++, ev->add_status, -1);
	ras_store_bind(priv->stmt_cxl_memory_module_event, cxl_memory_module_event_fields, idx++, ev->event_sub_type, -1);
	ras_store_bind(priv->stmt_cxl_memory_module_event, cxl_memory_module_event_fields, idx++, (uint64_t)ev->comp_id, CXL_EVENT_GEN_MED_COMP_ID_SIZE);
	ras_store_bind(priv->stmt_cxl_memory_module_event, cxl_memory_module_event_fields, idx++, (uint64_t)ev->entity_id, CXL_PLDM_ENTITY_ID_LEN);
	ras_store_bind(priv->stmt_cxl_memory_module_event, cxl_memory_module_event_fields, idx++, (uint64_t)ev->res_id, CXL_PLDM_RES_ID_LEN);

	rc = sqlite3_step(priv->stmt_cxl_memory_module_event);
	if (rc != SQLITE_DONE)
		log(TERM, LOG_ERR,
		    "Failed to do stmt_cxl_memory_module_event step on sqlite: error = %d\n", rc);
	rc = sqlite3_reset(priv->stmt_cxl_memory_module_event);
	if (rc != SQLITE_OK)
		log(TERM, LOG_ERR,
		    "Failed reset stmt_cxl_memory_module_event on sqlite: error = %d\n", rc);
	log(TERM, LOG_INFO, "register inserted at db\n");

	return rc;
}
#endif

#ifdef HAVE_SIGNAL
static const struct db_fields signal_event_fields[] = {
	{ .name = "id",		.type = DB_TYPE_SERIAL, .is_pk = true },
	{ .name = "timestamp",	.type = DB_TYPE_TEXT },
	{ .name = "sig",	.type = DB_TYPE_INT32 },
	{ .name = "errorno",	.type = DB_TYPE_INT32 },
	{ .name = "code",	.type = DB_TYPE_INT32 },
	{ .name = "comm",	.type = DB_TYPE_TEXT },
	{ .name = "pid",	.type = DB_TYPE_INT32 },
	{ .name = "grp",	.type = DB_TYPE_INT32 },
	{ .name = "res",	.type = DB_TYPE_INT32 },
};

static const struct db_table_descriptor signal_event_tab = {
	.name = "signal_event",
	.fields = signal_event_fields,
	.num_fields = ARRAY_SIZE(signal_event_fields),
};

int ras_store_signal_event(struct ras_events *ras, struct ras_signal_event *ev)
{
	int rc;
	struct sqlite3_priv *priv = ras->db_priv;

	if (!priv || !priv->stmt_signal_event)
		return -1;
	log(TERM, LOG_INFO, "signal_event store: %p\n", priv->stmt_signal_event);

	ras_store_bind(priv->stmt_signal_event, signal_event_fields, 1, (uint64_t)ev->timestamp, -1);
	ras_store_bind(priv->stmt_signal_event, signal_event_fields, 2, ev->sig, -1);
	ras_store_bind(priv->stmt_signal_event, signal_event_fields, 3, ev->error_no, -1);
	ras_store_bind(priv->stmt_signal_event, signal_event_fields, 4, ev->code, -1);
	ras_store_bind(priv->stmt_signal_event, signal_event_fields, 5, (uint64_t)ev->comm, -1);
	ras_store_bind(priv->stmt_signal_event, signal_event_fields, 6, ev->pid, -1);
	ras_store_bind(priv->stmt_signal_event, signal_event_fields, 7, ev->group, -1);
	ras_store_bind(priv->stmt_signal_event, signal_event_fields, 8, ev->result, -1);

	rc = sqlite3_step(priv->stmt_signal_event);
	if (rc != SQLITE_OK && rc != SQLITE_DONE)
		log(TERM, LOG_ERR,
		    "Failed to do signal_event step on sqlite: error = %d\n", rc);

	rc = sqlite3_reset(priv->stmt_signal_event);
	if (rc != SQLITE_OK && rc != SQLITE_DONE)
		log(TERM, LOG_ERR,
		    "Failed reset signal_event on sqlite: error = %d\n",
		    rc);

	log(TERM, LOG_INFO, "register inserted at db\n");

	return rc;
}
#endif

/*
 * Table and functions to handle ras:reri_event
 */

#ifdef HAVE_RERI
static const struct db_fields reri_event_fields[] = {
	{ .name = "id",			.type = DB_TYPE_SERIAL, .is_pk = true },
	{ .name = "timestamp",		.type = DB_TYPE_TEXT },
	{ .name = "err_src_id",		.type = DB_TYPE_INT32 },
	{ .name = "source_type",	.type = DB_TYPE_INT32 },
	{ .name = "severity",		.type = DB_TYPE_INT32 },
	{ .name = "hart_id",		.type = DB_TYPE_INT32 },
	{ .name = "cluster_id",		.type = DB_TYPE_INT32 },
	{ .name = "status",		.type = DB_TYPE_INT64 },
	{ .name = "addr_info",		.type = DB_TYPE_INT64 },
	{ .name = "info",		.type = DB_TYPE_INT64 },
	{ .name = "suppl_info",		.type = DB_TYPE_INT64 },
	{ .name = "timestamp_val",	.type = DB_TYPE_INT64 },
};

static const struct db_table_descriptor reri_event_tab = {
	.name = "reri_event",
	.fields = reri_event_fields,
	.num_fields = ARRAY_SIZE(reri_event_fields),
};

int ras_store_reri_event(struct ras_events *ras, struct ras_reri_event *ev)
{
	int rc;
	struct sqlite3_priv *priv = ras->db_priv;

	if (!priv || !priv->stmt_reri_event)
		return 0;
	log(TERM, LOG_INFO, "reri_event store: %p\n", priv->stmt_reri_event);

	ras_store_bind(priv->stmt_reri_event, reri_event_fields, 1, (uint64_t)ev->timestamp, -1);
	ras_store_bind(priv->stmt_reri_event, reri_event_fields, 2, ev->err_src_id, -1);
	ras_store_bind(priv->stmt_reri_event, reri_event_fields, 3, ev->source_type, -1);
	ras_store_bind(priv->stmt_reri_event, reri_event_fields, 4, ev->severity, -1);
	ras_store_bind(priv->stmt_reri_event, reri_event_fields, 5, ev->hart_id, -1);
	ras_store_bind(priv->stmt_reri_event, reri_event_fields, 6, ev->cluster_id, -1);
	ras_store_bind(priv->stmt_reri_event, reri_event_fields, 7, ev->status, -1);
	ras_store_bind(priv->stmt_reri_event, reri_event_fields, 8, ev->addr_info, -1);
	ras_store_bind(priv->stmt_reri_event, reri_event_fields, 9, ev->info, -1);
	ras_store_bind(priv->stmt_reri_event, reri_event_fields, 10, ev->suppl_info, -1);
	ras_store_bind(priv->stmt_reri_event, reri_event_fields, 11, ev->timestamp_val, -1);

	rc = sqlite3_step(priv->stmt_reri_event);
	if (rc != SQLITE_OK && rc != SQLITE_DONE)
		log(TERM, LOG_ERR,
		    "Failed to do reri_event step on sqlite: error = %d\n", rc);
	rc = sqlite3_reset(priv->stmt_reri_event);
	if (rc != SQLITE_OK && rc != SQLITE_DONE)
		log(TERM, LOG_ERR,
		    "Failed reset reri_event on sqlite: error = %d\n", rc);
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
			strscat(sql, "?, ", sizeof(sql));
		else
			strscat(sql, "?)", sizeof(sql));
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
	char sql[1024], *p = sql, *end = sql + sizeof(sql);
	const struct db_fields *field;
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
	const char *type;
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
			type = db_get_sql_type(field->type, field->is_pk);

			/* add new field */
			p += snprintf(p, end - p, "ALTER TABLE %s ADD ",
				      db_tab->name);
			p += snprintf(p, end - p,
				      "%s %s", field->name, type);
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

	/*
	 * on sqlite3, SQLITE_OK is actually zero, but let's do it to
	 * stabilish a generic API contract: returning zero here means no
	 * error.
	 */
	if (rc == SQLITE_OK)
		return 0;

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

int ras_mc_event_opendb(unsigned int cpu, struct ras_events *ras)
{
	int rc;
	sqlite3 *db;
	struct sqlite3_priv *priv;

	printf("Calling %s()\n", __func__);

	ras->db_ref_count++;
	if (ras->db_ref_count > 1)
		return 0;

	ras->db_priv = NULL;

	priv = calloc(1, sizeof(*priv));
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

#ifdef HAVE_MEMORY_FAILURE
	rc = ras_mc_create_table(priv, &mf_event_tab);
	if (rc == SQLITE_OK) {
		rc = ras_mc_prepare_stmt(priv, &priv->stmt_mf_event,
					 &mf_event_tab);
		if (rc != SQLITE_OK)
			goto error;
	}
#endif

#ifdef HAVE_CXL
	rc = ras_mc_create_table(priv, &cxl_poison_event_tab);
	if (rc == SQLITE_OK) {
		rc = ras_mc_prepare_stmt(priv, &priv->stmt_cxl_poison_event,
					 &cxl_poison_event_tab);
		if (rc != SQLITE_OK)
			goto error;
	}

	rc = ras_mc_create_table(priv, &cxl_aer_ue_event_tab);
	if (rc == SQLITE_OK) {
		rc = ras_mc_prepare_stmt(priv, &priv->stmt_cxl_aer_ue_event,
					 &cxl_aer_ue_event_tab);
		if (rc != SQLITE_OK)
			goto error;
	}

	rc = ras_mc_create_table(priv, &cxl_aer_ce_event_tab);
	if (rc == SQLITE_OK) {
		rc = ras_mc_prepare_stmt(priv, &priv->stmt_cxl_aer_ce_event,
					 &cxl_aer_ce_event_tab);
		if (rc != SQLITE_OK)
			goto error;
	}

	rc = ras_mc_create_table(priv, &cxl_overflow_event_tab);
	if (rc == SQLITE_OK) {
		rc = ras_mc_prepare_stmt(priv, &priv->stmt_cxl_overflow_event,
					 &cxl_overflow_event_tab);
		if (rc != SQLITE_OK)
			goto error;
	}

	rc = ras_mc_create_table(priv, &cxl_generic_event_tab);
	if (rc == SQLITE_OK) {
		rc = ras_mc_prepare_stmt(priv, &priv->stmt_cxl_generic_event,
					 &cxl_generic_event_tab);
		if (rc != SQLITE_OK)
			goto error;
	}

	rc = ras_mc_create_table(priv, &cxl_general_media_event_tab);
	if (rc == SQLITE_OK) {
		rc = ras_mc_prepare_stmt(priv, &priv->stmt_cxl_general_media_event,
					 &cxl_general_media_event_tab);
		if (rc != SQLITE_OK)
			goto error;
	}

	rc = ras_mc_create_table(priv, &cxl_dram_event_tab);
	if (rc == SQLITE_OK) {
		rc = ras_mc_prepare_stmt(priv, &priv->stmt_cxl_dram_event,
					 &cxl_dram_event_tab);
		if (rc != SQLITE_OK)
			goto error;
	}

	rc = ras_mc_create_table(priv, &cxl_memory_module_event_tab);
	if (rc == SQLITE_OK) {
		rc = ras_mc_prepare_stmt(priv, &priv->stmt_cxl_memory_module_event,
					 &cxl_memory_module_event_tab);
		if (rc != SQLITE_OK)
			goto error;
	}
#endif

#ifdef HAVE_SIGNAL
	rc = ras_mc_create_table(priv, &signal_event_tab);
	if (rc == SQLITE_OK) {
		rc = ras_mc_prepare_stmt(priv, &priv->stmt_signal_event,
					 &signal_event_tab);
		if (rc != SQLITE_OK)
			goto error;
	}
#endif

#ifdef HAVE_RERI
	rc = ras_mc_create_table(priv, &reri_event_tab);
	if (rc == SQLITE_OK) {
		rc = ras_mc_prepare_stmt(priv, &priv->stmt_reri_event,
					 &reri_event_tab);
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

	if (ras->db_ref_count > 0)
		ras->db_ref_count--;
	else
		return -1;
	if (ras->db_ref_count > 0)
		return 0;

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

#ifdef HAVE_MEMORY_FAILURE
	if (priv->stmt_mf_event) {
		rc = sqlite3_finalize(priv->stmt_mf_event);
		if (rc != SQLITE_OK)
			log(TERM, LOG_ERR,
			    "cpu %u: Failed to finalize mf_event sqlite: error = %d\n",
			    cpu, rc);
	}
#endif

#ifdef HAVE_CXL
	if (priv->stmt_cxl_poison_event) {
		rc = sqlite3_finalize(priv->stmt_cxl_poison_event);
		if (rc != SQLITE_OK)
			log(TERM, LOG_ERR,
			    "cpu %u: Failed to finalize cxl_poison_event sqlite: error = %d\n",
			    cpu, rc);
	}

	if (priv->stmt_cxl_aer_ue_event) {
		rc = sqlite3_finalize(priv->stmt_cxl_aer_ue_event);
		if (rc != SQLITE_OK)
			log(TERM, LOG_ERR,
			    "cpu %u: Failed to finalize cxl_aer_ue_event sqlite: error = %d\n",
			    cpu, rc);
	}

	if (priv->stmt_cxl_aer_ce_event) {
		rc = sqlite3_finalize(priv->stmt_cxl_aer_ce_event);
		if (rc != SQLITE_OK)
			log(TERM, LOG_ERR,
			    "cpu %u: Failed to finalize cxl_aer_ce_event sqlite: error = %d\n",
			    cpu, rc);
	}

	if (priv->stmt_cxl_overflow_event) {
		rc = sqlite3_finalize(priv->stmt_cxl_overflow_event);
		if (rc != SQLITE_OK)
			log(TERM, LOG_ERR,
			    "cpu %u: Failed to finalize cxl_overflow_event sqlite: error = %d\n",
			    cpu, rc);
	}

	if (priv->stmt_cxl_generic_event) {
		rc = sqlite3_finalize(priv->stmt_cxl_generic_event);
		if (rc != SQLITE_OK)
			log(TERM, LOG_ERR,
			    "cpu %u: Failed to finalize cxl_generic_event sqlite: error = %d\n",
			    cpu, rc);
	}

	if (priv->stmt_cxl_general_media_event) {
		rc = sqlite3_finalize(priv->stmt_cxl_general_media_event);
		if (rc != SQLITE_OK)
			log(TERM, LOG_ERR,
			    "cpu %u: Failed to finalize cxl_general_media_event sqlite: error = %d\n",
			    cpu, rc);
	}

	if (priv->stmt_cxl_dram_event) {
		rc = sqlite3_finalize(priv->stmt_cxl_dram_event);
		if (rc != SQLITE_OK)
			log(TERM, LOG_ERR,
			    "cpu %u: Failed to finalize cxl_dram_event sqlite: error = %d\n",
			    cpu, rc);
	}

	if (priv->stmt_cxl_memory_module_event) {
		rc = sqlite3_finalize(priv->stmt_cxl_memory_module_event);
		if (rc != SQLITE_OK)
			log(TERM, LOG_ERR,
			    "cpu %u: Failed to finalize stmt_cxl_memory_module_event sqlite: error = %d\n",
			    cpu, rc);
	}
#endif

#ifdef HAVE_SIGNAL
	if (priv->stmt_signal_event) {
		rc = sqlite3_finalize(priv->stmt_signal_event);
		if (rc != SQLITE_OK)
			log(TERM, LOG_ERR,
			    "cpu %u: Failed to finalize signal_event sqlite: error = %d\n",
			    cpu, rc);
	}
#endif

#ifdef HAVE_RERI
	if (priv->stmt_reri_event) {
		rc = sqlite3_finalize(priv->stmt_reri_event);
		if (rc != SQLITE_OK)
			log(TERM, LOG_ERR,
			    "cpu %u: Failed to finalize reri_event sqlite: error = %d\n",
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
	ras->db_priv = NULL;

	return 0;
}
