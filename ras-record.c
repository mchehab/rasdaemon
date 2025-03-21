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

#include "ras-aer-handler.h"
#include "ras-events.h"
#include "ras-logger.h"
#include "ras-mce-handler.h"
#include "ras-mc-handler.h"
#include "ras-record.h"

/*
 * BuildRequires: sqlite-devel
 */

/* #define DEBUG_SQL 1 */

#define SQLITE_RAS_DB RASSTATEDIR "/" RAS_DB_FNAME

/*
 * Table and functions to handle ras:mc_event
 */

static const struct db_fields mc_event_fields[] = {
		{ .name = "id",			.type = "INTEGER PRIMARY KEY" },
		{ .name = "timestamp",		.type = "TEXT" },
		{ .name = "err_count",		.type = "INTEGER" },
		{ .name = "err_type",		.type = "TEXT" },
		{ .name = "err_msg",		.type = "TEXT" },
		{ .name = "label",		.type = "TEXT" },
		{ .name = "mc",			.type = "INTEGER" },
		{ .name = "top_layer",		.type = "INTEGER" },
		{ .name = "middle_layer",		.type = "INTEGER" },
		{ .name = "lower_layer",		.type = "INTEGER" },
		{ .name = "address",		.type = "INTEGER" },
		{ .name = "grain",		.type = "INTEGER" },
		{ .name = "syndrome",		.type = "INTEGER" },
		{ .name = "driver_detail",	.type = "TEXT" },
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
	sqlite3_bind_int64(priv->stmt_mc_event, 10, ev->address);
	sqlite3_bind_int64(priv->stmt_mc_event, 11, ev->grain);
	sqlite3_bind_int64(priv->stmt_mc_event, 12, ev->syndrome);
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
		{ .name = "id",			.type = "INTEGER PRIMARY KEY" },
		{ .name = "timestamp",		.type = "TEXT" },
		{ .name = "dev_name",		.type = "TEXT" },
		{ .name = "err_type",		.type = "TEXT" },
		{ .name = "err_msg",		.type = "TEXT" },
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
		{ .name = "id",			.type = "INTEGER PRIMARY KEY" },
		{ .name = "timestamp",		.type = "TEXT" },
		{ .name = "sec_type",		.type = "BLOB" },
		{ .name = "fru_id",		.type = "BLOB" },
		{ .name = "fru_text",		.type = "TEXT" },
		{ .name = "severity",		.type = "TEXT" },
		{ .name = "error",		.type = "BLOB" },
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

	sqlite3_bind_text(priv->stmt_non_standard_record,  1, ev->timestamp, -1, NULL);
	sqlite3_bind_blob(priv->stmt_non_standard_record,  2, ev->sec_type, -1, NULL);
	sqlite3_bind_blob(priv->stmt_non_standard_record,  3, ev->fru_id, 16, NULL);
	sqlite3_bind_text(priv->stmt_non_standard_record,  4, ev->fru_text, -1, NULL);
	sqlite3_bind_text(priv->stmt_non_standard_record,  5, ev->severity, -1, NULL);
	sqlite3_bind_blob(priv->stmt_non_standard_record,  6, ev->error, ev->length, NULL);

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
		{ .name = "id",			.type = "INTEGER PRIMARY KEY" },
		{ .name = "timestamp",		.type = "TEXT" },
		{ .name = "error_count",		.type = "INTEGER" },
		{ .name = "affinity",		.type = "INTEGER" },
		{ .name = "mpidr",		.type = "INTEGER" },
		{ .name = "running_state",	.type = "INTEGER" },
		{ .name = "psci_state",		.type = "INTEGER" },
		{ .name = "err_info",		.type = "BLOB"	},
		{ .name = "context_info",		.type = "BLOB"	},
		{ .name = "vendor_info",		.type = "BLOB"	},
		{ .name = "error_type",		.type = "TEXT" },
		{ .name = "error_flags",	.type = "TEXT" },
		{ .name = "error_info",		.type = "INTEGER" },
		{ .name = "virt_fault_addr",	.type = "INTEGER" },
		{ .name = "phy_fault_addr",	.type = "INTEGER" },
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

	sqlite3_bind_text(priv->stmt_arm_record,  1,  ev->timestamp, -1, NULL);
	sqlite3_bind_int  (priv->stmt_arm_record,  2,  ev->error_count);
	sqlite3_bind_int  (priv->stmt_arm_record,  3,  ev->affinity);
	sqlite3_bind_int64(priv->stmt_arm_record,  4,  ev->mpidr);
	sqlite3_bind_int  (priv->stmt_arm_record,  5,  ev->running_state);
	sqlite3_bind_int  (priv->stmt_arm_record,  6,  ev->psci_state);
	sqlite3_bind_blob(priv->stmt_arm_record,  7,
			  ev->pei_error, ev->pei_len, NULL);
	sqlite3_bind_blob(priv->stmt_arm_record,  8,
			  ev->ctx_error, ev->ctx_len, NULL);
	sqlite3_bind_blob(priv->stmt_arm_record,  9,
			  ev->vsei_error, ev->oem_len, NULL);
	sqlite3_bind_text(priv->stmt_arm_record,  10, ev->error_types, -1, NULL);
	sqlite3_bind_text(priv->stmt_arm_record, 11, ev->error_flags, -1, NULL);
	sqlite3_bind_int64(priv->stmt_arm_record,  12,  ev->error_info);
	sqlite3_bind_int64(priv->stmt_arm_record,  13,  ev->virt_fault_addr);
	sqlite3_bind_int64(priv->stmt_arm_record,  14,  ev->phy_fault_addr);

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
		{ .name = "id",			.type = "INTEGER PRIMARY KEY" },
		{ .name = "timestamp",		.type = "TEXT" },
		{ .name = "etype",		.type = "INTEGER" },
		{ .name = "error_count",		.type = "INTEGER" },
		{ .name = "severity",		.type = "INTEGER" },
		{ .name = "address",		.type = "INTEGER" },
		{ .name = "fru_id",		.type = "BLOB" },
		{ .name = "fru_text",		.type = "TEXT" },
		{ .name = "cper_data",		.type = "BLOB" },
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

	sqlite3_bind_text(priv->stmt_extlog_record,  1, ev->timestamp, -1, NULL);
	sqlite3_bind_int   (priv->stmt_extlog_record,  2, ev->etype);
	sqlite3_bind_int   (priv->stmt_extlog_record,  3, ev->error_seq);
	sqlite3_bind_int   (priv->stmt_extlog_record,  4, ev->severity);
	sqlite3_bind_int64(priv->stmt_extlog_record,  5, ev->address);
	sqlite3_bind_blob(priv->stmt_extlog_record,  6, ev->fru_id, 16, NULL);
	sqlite3_bind_text(priv->stmt_extlog_record,  7, ev->fru_text, -1, NULL);
	sqlite3_bind_blob(priv->stmt_extlog_record,  8, ev->cper_data, ev->cper_data_length, NULL);

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
		{ .name = "id",			.type = "INTEGER PRIMARY KEY" },
		{ .name = "timestamp",		.type = "TEXT" },

		/* MCE registers */
		{ .name = "mcgcap",		.type = "INTEGER" },
		{ .name = "mcgstatus",		.type = "INTEGER" },
		{ .name = "status",		.type = "INTEGER" },
		{ .name = "addr",			.type = "INTEGER" }, // 5
		{ .name = "misc",			.type = "INTEGER" },
		{ .name = "ip",			.type = "INTEGER" },
		{ .name = "tsc",			.type = "INTEGER" },
		{ .name = "walltime",		.type = "INTEGER" },
		{ .name = "ppin",			.type = "INTEGER" }, // 10
		{ .name = "cpu",			.type = "INTEGER" },
		{ .name = "cpuid",		.type = "INTEGER" },
		{ .name = "apicid",		.type = "INTEGER" },
		{ .name = "socketid",		.type = "INTEGER" },
		{ .name = "cs",			.type = "INTEGER" }, // 15
		{ .name = "bank",			.type = "INTEGER" },
		{ .name = "cpuvendor",		.type = "INTEGER" },
		{ .name = "microcode",      .type = "INTEGER" },

		/* Parsed data - will likely change */
		{ .name = "bank_name",		.type = "TEXT" },
		{ .name = "error_msg",		.type = "TEXT" }, // 20
		{ .name = "mcgstatus_msg",	.type = "TEXT" },
		{ .name = "mcistatus_msg",	.type = "TEXT" },
		{ .name = "mcastatus_msg",	.type = "TEXT" },
		{ .name = "user_action",		.type = "TEXT" },
		{ .name = "mc_location",		.type = "TEXT" },
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

	sqlite3_bind_text(priv->stmt_mce_record,  1, ev->timestamp, -1, NULL);
	sqlite3_bind_int   (priv->stmt_mce_record,  2, ev->mcgcap);
	sqlite3_bind_int   (priv->stmt_mce_record,  3, ev->mcgstatus);
	sqlite3_bind_int64(priv->stmt_mce_record,  4, ev->status);
	sqlite3_bind_int64(priv->stmt_mce_record,  5, ev->addr);
	sqlite3_bind_int64(priv->stmt_mce_record,  6, ev->misc);
	sqlite3_bind_int64(priv->stmt_mce_record,  7, ev->ip);
	sqlite3_bind_int64(priv->stmt_mce_record,  8, ev->tsc);
	sqlite3_bind_int64(priv->stmt_mce_record,  9, ev->walltime);
	sqlite3_bind_int64(priv->stmt_mce_record,  10, ev->ppin);
	sqlite3_bind_int   (priv->stmt_mce_record, 11, ev->cpu);
	sqlite3_bind_int   (priv->stmt_mce_record, 12, ev->cpuid);
	sqlite3_bind_int   (priv->stmt_mce_record, 13, ev->apicid);
	sqlite3_bind_int   (priv->stmt_mce_record, 14, ev->socketid);
	sqlite3_bind_int   (priv->stmt_mce_record, 15, ev->cs);
	sqlite3_bind_int   (priv->stmt_mce_record, 16, ev->bank);
	sqlite3_bind_int   (priv->stmt_mce_record, 17, ev->cpuvendor);
	sqlite3_bind_int   (priv->stmt_mce_record, 18, ev->microcode);

	sqlite3_bind_text(priv->stmt_mce_record, 19, ev->bank_name, -1, NULL);
	sqlite3_bind_text(priv->stmt_mce_record, 20, ev->error_msg, -1, NULL);
	sqlite3_bind_text(priv->stmt_mce_record, 21, ev->mcgstatus_msg, -1, NULL);
	sqlite3_bind_text(priv->stmt_mce_record, 22, ev->mcistatus_msg, -1, NULL);
	sqlite3_bind_text(priv->stmt_mce_record, 23, ev->mcastatus_msg, -1, NULL);
	sqlite3_bind_text(priv->stmt_mce_record, 24, ev->user_action, -1, NULL);
	sqlite3_bind_text(priv->stmt_mce_record, 25, ev->mc_location, -1, NULL);

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
		{ .name = "id",			.type = "INTEGER PRIMARY KEY" },
		{ .name = "timestamp",		.type = "TEXT" },
		{ .name = "bus_name",		.type = "TEXT" },
		{ .name = "dev_name",		.type = "TEXT" },
		{ .name = "driver_name",		.type = "TEXT" },
		{ .name = "reporter_name",	.type = "TEXT" },
		{ .name = "msg",			.type = "TEXT" },
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
 * Table and functions to handle block:block_rq_{complete|error}
 */

#ifdef HAVE_DISKERROR
static const struct db_fields diskerror_event_fields[] = {
		{ .name = "id",			.type = "INTEGER PRIMARY KEY" },
		{ .name = "timestamp",		.type = "TEXT" },
		{ .name = "dev",			.type = "TEXT" },
		{ .name = "sector",		.type = "INTEGER" },
		{ .name = "nr_sector",		.type = "INTEGER" },
		{ .name = "error",		.type = "TEXT" },
		{ .name = "rwbs",			.type = "TEXT" },
		{ .name = "cmd",			.type = "TEXT" },
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
 * Table and functions to handle ras:memory_failure
 */

#ifdef HAVE_MEMORY_FAILURE
static const struct db_fields mf_event_fields[] = {
	{ .name = "id",			.type = "INTEGER PRIMARY KEY" },
	{ .name = "timestamp",		.type = "TEXT" },
	{ .name = "pfn",		.type = "TEXT" },
	{ .name = "page_type",		.type = "TEXT" },
	{ .name = "action_result",	.type = "TEXT" },
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

	sqlite3_bind_text(priv->stmt_mf_event,  1, ev->timestamp, -1, NULL);
	sqlite3_bind_text(priv->stmt_mf_event,  2, ev->pfn, -1, NULL);
	sqlite3_bind_text(priv->stmt_mf_event,  3, ev->page_type, -1, NULL);
	sqlite3_bind_text(priv->stmt_mf_event,  4, ev->action_result, -1, NULL);

	rc = sqlite3_step(priv->stmt_mf_event);
	if (rc != SQLITE_OK && rc != SQLITE_DONE)
		log(TERM, LOG_ERR,
		    "Failed to do memory_failure_event step on sqlite: error = %d\n", rc);

	rc = sqlite3_reset(priv->stmt_mf_event);
	if (rc != SQLITE_OK && rc != SQLITE_DONE)
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
	{ .name = "id",			.type = "INTEGER PRIMARY KEY" },
	{ .name = "timestamp",		.type = "TEXT" },
	{ .name = "memdev",		.type = "TEXT" },
	{ .name = "host",		.type = "TEXT" },
	{ .name = "serial",		.type = "INTEGER" },
	{ .name = "trace_type",		.type = "TEXT" },
	{ .name = "region",		.type = "TEXT" },
	{ .name = "region_uuid",	.type = "TEXT" },
	{ .name = "hpa",		.type = "INTEGER" },
	{ .name = "dpa",		.type = "INTEGER" },
	{ .name = "dpa_length",		.type = "INTEGER" },
	{ .name = "source",		.type = "TEXT" },
	{ .name = "flags",		.type = "INTEGER" },
	{ .name = "overflow_ts",	.type = "TEXT" },
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

	sqlite3_bind_text(priv->stmt_cxl_poison_event, 1, ev->timestamp, -1, NULL);
	sqlite3_bind_text(priv->stmt_cxl_poison_event, 2, ev->memdev, -1, NULL);
	sqlite3_bind_text(priv->stmt_cxl_poison_event, 3, ev->host, -1, NULL);
	sqlite3_bind_int64(priv->stmt_cxl_poison_event, 4, ev->serial);
	sqlite3_bind_text(priv->stmt_cxl_poison_event, 5, ev->trace_type, -1, NULL);
	sqlite3_bind_text(priv->stmt_cxl_poison_event, 6, ev->region, -1, NULL);
	sqlite3_bind_text(priv->stmt_cxl_poison_event, 7, ev->uuid, -1, NULL);
	sqlite3_bind_int64(priv->stmt_cxl_poison_event, 8, ev->hpa);
	sqlite3_bind_int64(priv->stmt_cxl_poison_event, 9, ev->dpa);
	sqlite3_bind_int(priv->stmt_cxl_poison_event, 10, ev->dpa_length);
	sqlite3_bind_text(priv->stmt_cxl_poison_event, 11, ev->source, -1, NULL);
	sqlite3_bind_int(priv->stmt_cxl_poison_event, 12, ev->flags);
	sqlite3_bind_text(priv->stmt_cxl_poison_event, 13, ev->overflow_ts, -1, NULL);

	rc = sqlite3_step(priv->stmt_cxl_poison_event);
	if (rc != SQLITE_OK && rc != SQLITE_DONE)
		log(TERM, LOG_ERR,
		    "Failed to do cxl_poison_event step on sqlite: error = %d\n", rc);
	rc = sqlite3_reset(priv->stmt_cxl_poison_event);
	if (rc != SQLITE_OK && rc != SQLITE_DONE)
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
	{ .name = "id",			.type = "INTEGER PRIMARY KEY" },
	{ .name = "timestamp",		.type = "TEXT" },
	{ .name = "memdev",		.type = "TEXT" },
	{ .name = "host",		.type = "TEXT" },
	{ .name = "serial",		.type = "INTEGER" },
	{ .name = "error_status",	.type = "INTEGER" },
	{ .name = "first_error",	.type = "INTEGER" },
	{ .name = "header_log",		.type = "BLOB" },
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

	sqlite3_bind_text(priv->stmt_cxl_aer_ue_event, 1, ev->timestamp, -1, NULL);
	sqlite3_bind_text(priv->stmt_cxl_aer_ue_event, 2, ev->memdev, -1, NULL);
	sqlite3_bind_text(priv->stmt_cxl_aer_ue_event, 3, ev->host, -1, NULL);
	sqlite3_bind_int64(priv->stmt_cxl_aer_ue_event, 4, ev->serial);
	sqlite3_bind_int(priv->stmt_cxl_aer_ue_event, 5, ev->error_status);
	sqlite3_bind_int(priv->stmt_cxl_aer_ue_event, 6, ev->first_error);
	sqlite3_bind_blob(priv->stmt_cxl_aer_ue_event, 7, ev->header_log, CXL_HEADERLOG_SIZE, NULL);

	rc = sqlite3_step(priv->stmt_cxl_aer_ue_event);
	if (rc != SQLITE_OK && rc != SQLITE_DONE)
		log(TERM, LOG_ERR,
		    "Failed to do cxl_aer_ue_event step on sqlite: error = %d\n", rc);
	rc = sqlite3_reset(priv->stmt_cxl_aer_ue_event);
	if (rc != SQLITE_OK && rc != SQLITE_DONE)
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
	{ .name = "id",			.type = "INTEGER PRIMARY KEY" },
	{ .name = "timestamp",		.type = "TEXT" },
	{ .name = "memdev",		.type = "TEXT" },
	{ .name = "host",		.type = "TEXT" },
	{ .name = "serial",		.type = "INTEGER" },
	{ .name = "error_status",	.type = "INTEGER" },
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

	sqlite3_bind_text(priv->stmt_cxl_aer_ce_event, 1, ev->timestamp, -1, NULL);
	sqlite3_bind_text(priv->stmt_cxl_aer_ce_event, 2, ev->memdev, -1, NULL);
	sqlite3_bind_text(priv->stmt_cxl_aer_ce_event, 3, ev->host, -1, NULL);
	sqlite3_bind_int64(priv->stmt_cxl_aer_ce_event, 4, ev->serial);
	sqlite3_bind_int(priv->stmt_cxl_aer_ce_event, 5, ev->error_status);

	rc = sqlite3_step(priv->stmt_cxl_aer_ce_event);
	if (rc != SQLITE_OK && rc != SQLITE_DONE)
		log(TERM, LOG_ERR,
		    "Failed to do cxl_aer_ce_event step on sqlite: error = %d\n", rc);
	rc = sqlite3_reset(priv->stmt_cxl_aer_ce_event);
	if (rc != SQLITE_OK && rc != SQLITE_DONE)
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
	{ .name = "id",			.type = "INTEGER PRIMARY KEY" },
	{ .name = "timestamp",		.type = "TEXT" },
	{ .name = "memdev",		.type = "TEXT" },
	{ .name = "host",		.type = "TEXT" },
	{ .name = "serial",		.type = "INTEGER" },
	{ .name = "log_type",		.type = "TEXT" },
	{ .name = "count",		.type = "INTEGER" },
	{ .name = "first_ts",		.type = "TEXT" },
	{ .name = "last_ts",		.type = "TEXT" },
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

	sqlite3_bind_text(priv->stmt_cxl_overflow_event, 1, ev->timestamp, -1, NULL);
	sqlite3_bind_text(priv->stmt_cxl_overflow_event, 2, ev->memdev, -1, NULL);
	sqlite3_bind_text(priv->stmt_cxl_overflow_event, 3, ev->host, -1, NULL);
	sqlite3_bind_int64(priv->stmt_cxl_overflow_event, 4, ev->serial);
	sqlite3_bind_text(priv->stmt_cxl_overflow_event, 5, ev->log_type, -1, NULL);
	sqlite3_bind_int(priv->stmt_cxl_overflow_event, 6, ev->count);
	sqlite3_bind_text(priv->stmt_cxl_overflow_event, 7, ev->first_ts, -1, NULL);
	sqlite3_bind_text(priv->stmt_cxl_overflow_event, 8, ev->last_ts, -1, NULL);

	rc = sqlite3_step(priv->stmt_cxl_overflow_event);
	if (rc != SQLITE_OK && rc != SQLITE_DONE)
		log(TERM, LOG_ERR,
		    "Failed to do cxl_overflow_event step on sqlite: error = %d\n", rc);
	rc = sqlite3_reset(priv->stmt_cxl_overflow_event);
	if (rc != SQLITE_OK && rc != SQLITE_DONE)
		log(TERM, LOG_ERR,
		    "Failed reset cxl_overflow_event on sqlite: error = %d\n",
		    rc);
	log(TERM, LOG_INFO, "register inserted at db\n");

	return rc;
}

static int ras_store_cxl_common_hdr(sqlite3_stmt *stmt, struct ras_cxl_event_common_hdr *hdr)
{
	int idx = 1;

	if (!stmt || !hdr)
		return -1;

	sqlite3_bind_text(stmt, idx++, hdr->timestamp, -1, NULL);
	sqlite3_bind_text(stmt, idx++, hdr->memdev, -1, NULL);
	sqlite3_bind_text(stmt, idx++, hdr->host, -1, NULL);
	sqlite3_bind_int64(stmt, idx++, hdr->serial);
	sqlite3_bind_text(stmt, idx++, hdr->log_type, -1, NULL);
	sqlite3_bind_text(stmt, idx++, hdr->hdr_uuid, -1, NULL);
	sqlite3_bind_int(stmt, idx++, hdr->hdr_flags);
	sqlite3_bind_int(stmt, idx++, hdr->hdr_handle);
	sqlite3_bind_int(stmt, idx++, hdr->hdr_related_handle);
	sqlite3_bind_text(stmt, idx++, hdr->hdr_timestamp, -1, NULL);
	sqlite3_bind_int(stmt, idx++, hdr->hdr_length);
	sqlite3_bind_int(stmt, idx++, hdr->hdr_maint_op_class);
	sqlite3_bind_int(stmt, idx++, hdr->hdr_maint_op_sub_class);

	return idx;
}

/*
 * Table and functions to handle cxl:cxl_generic_event
 */
static const struct db_fields cxl_generic_event_fields[] = {
	{ .name = "id",			.type = "INTEGER PRIMARY KEY" },
	{ .name = "timestamp",		.type = "TEXT" },
	{ .name = "memdev",		.type = "TEXT" },
	{ .name = "host",		.type = "TEXT" },
	{ .name = "serial",		.type = "INTEGER" },
	{ .name = "log_type",		.type = "TEXT" },
	{ .name = "hdr_uuid",		.type = "TEXT" },
	{ .name = "hdr_flags",		.type = "INTEGER" },
	{ .name = "hdr_handle",		.type = "INTEGER" },
	{ .name = "hdr_related_handle",	.type = "INTEGER" },
	{ .name = "hdr_ts",		.type = "TEXT" },
	{ .name = "hdr_length",		.type = "INTEGER" },
	{ .name = "hdr_maint_op_class",	.type = "INTEGER" },
	{ .name = "hdr_maint_op_sub_class",	.type = "INTEGER" },
	{ .name = "data",		.type = "BLOB" },
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

	idx = ras_store_cxl_common_hdr(priv->stmt_cxl_generic_event, &ev->hdr);
	if (idx <= 0)
		return -1;

	sqlite3_bind_blob(priv->stmt_cxl_generic_event, idx++, ev->data,
			  CXL_EVENT_RECORD_DATA_LENGTH, NULL);

	rc = sqlite3_step(priv->stmt_cxl_generic_event);
	if (rc != SQLITE_OK && rc != SQLITE_DONE)
		log(TERM, LOG_ERR,
		    "Failed to do stmt_cxl_generic_event step on sqlite: error = %d\n", rc);
	rc = sqlite3_reset(priv->stmt_cxl_generic_event);
	if (rc != SQLITE_OK && rc != SQLITE_DONE)
		log(TERM, LOG_ERR,
		    "Failed reset stmt_cxl_generic_event on sqlite: error = %d\n", rc);
	log(TERM, LOG_INFO, "register inserted at db\n");

	return rc;
}

/*
 * Table and functions to handle cxl:cxl_general_media_event
 */
static const struct db_fields cxl_general_media_event_fields[] = {
	{ .name = "id",			.type = "INTEGER PRIMARY KEY" },
	{ .name = "timestamp",		.type = "TEXT" },
	{ .name = "memdev",		.type = "TEXT" },
	{ .name = "host",		.type = "TEXT" },
	{ .name = "serial",		.type = "INTEGER" },
	{ .name = "log_type",		.type = "TEXT" },
	{ .name = "hdr_uuid",		.type = "TEXT" },
	{ .name = "hdr_flags",		.type = "INTEGER" },
	{ .name = "hdr_handle",		.type = "INTEGER" },
	{ .name = "hdr_related_handle",	.type = "INTEGER" },
	{ .name = "hdr_ts",		.type = "TEXT" },
	{ .name = "hdr_length",		.type = "INTEGER" },
	{ .name = "hdr_maint_op_class",	.type = "INTEGER" },
	{ .name = "hdr_maint_op_sub_class",	.type = "INTEGER" },
	{ .name = "dpa",		.type = "INTEGER" },
	{ .name = "dpa_flags",		.type = "INTEGER" },
	{ .name = "descriptor",		.type = "INTEGER" },
	{ .name = "type",		.type = "INTEGER" },
	{ .name = "transaction_type",	.type = "INTEGER" },
	{ .name = "channel",		.type = "INTEGER" },
	{ .name = "rank",		.type = "INTEGER" },
	{ .name = "device",		.type = "INTEGER" },
	{ .name = "comp_id",		.type = "BLOB" },
	{ .name = "hpa",		.type = "INTEGER" },
	{ .name = "region",		.type = "TEXT" },
	{ .name = "region_uuid",	.type = "TEXT" },
	{ .name = "pldm_entity_id",	.type = "BLOB" },
	{ .name = "pldm_resource_id",	.type = "BLOB" },
	{ .name = "sub_type",		.type = "INTEGER" },
	{ .name = "cme_threshold_ev_flags",	.type = "INTEGER" },
	{ .name = "cme_count",		.type = "INTEGER" },
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

	idx = ras_store_cxl_common_hdr(priv->stmt_cxl_general_media_event, &ev->hdr);
	if (idx <= 0)
		return -1;
	sqlite3_bind_int64(priv->stmt_cxl_general_media_event, idx++, ev->dpa);
	sqlite3_bind_int(priv->stmt_cxl_general_media_event, idx++, ev->dpa_flags);
	sqlite3_bind_int(priv->stmt_cxl_general_media_event, idx++, ev->descriptor);
	sqlite3_bind_int(priv->stmt_cxl_general_media_event, idx++, ev->type);
	sqlite3_bind_int(priv->stmt_cxl_general_media_event, idx++, ev->transaction_type);
	sqlite3_bind_int(priv->stmt_cxl_general_media_event, idx++, ev->channel);
	sqlite3_bind_int(priv->stmt_cxl_general_media_event, idx++, ev->rank);
	sqlite3_bind_int(priv->stmt_cxl_general_media_event, idx++, ev->device);
	sqlite3_bind_blob(priv->stmt_cxl_general_media_event, idx++, ev->comp_id,
			  CXL_EVENT_GEN_MED_COMP_ID_SIZE, NULL);
	sqlite3_bind_int64(priv->stmt_cxl_general_media_event, idx++, ev->hpa);
	sqlite3_bind_text(priv->stmt_cxl_general_media_event, idx++, ev->region, -1, NULL);
	sqlite3_bind_text(priv->stmt_cxl_general_media_event, idx++, ev->region_uuid, -1, NULL);
	sqlite3_bind_blob(priv->stmt_cxl_general_media_event, idx++, ev->entity_id,
			  CXL_PLDM_ENTITY_ID_LEN, NULL);
	sqlite3_bind_blob(priv->stmt_cxl_general_media_event, idx++, ev->res_id,
			  CXL_PLDM_RES_ID_LEN, NULL);
	sqlite3_bind_int(priv->stmt_cxl_general_media_event, idx++, ev->sub_type);
	sqlite3_bind_int(priv->stmt_cxl_general_media_event, idx++,
			 ev->cme_threshold_ev_flags);
	sqlite3_bind_int(priv->stmt_cxl_general_media_event, idx++, ev->cme_count);

	rc = sqlite3_step(priv->stmt_cxl_general_media_event);
	if (rc != SQLITE_OK && rc != SQLITE_DONE)
		log(TERM, LOG_ERR,
		    "Failed to do stmt_cxl_general_media_event step on sqlite: error = %d\n", rc);
	rc = sqlite3_reset(priv->stmt_cxl_general_media_event);
	if (rc != SQLITE_OK && rc != SQLITE_DONE)
		log(TERM, LOG_ERR,
		    "Failed reset stmt_cxl_general_media_event on sqlite: error = %d\n", rc);
	log(TERM, LOG_INFO, "register inserted at db\n");

	return rc;
}

/*
 * Table and functions to handle cxl:cxl_dram_event
 */
static const struct db_fields cxl_dram_event_fields[] = {
	{ .name = "id",			.type = "INTEGER PRIMARY KEY" },
	{ .name = "timestamp",		.type = "TEXT" },
	{ .name = "memdev",		.type = "TEXT" },
	{ .name = "host",		.type = "TEXT" },
	{ .name = "serial",		.type = "INTEGER" },
	{ .name = "log_type",		.type = "TEXT" },
	{ .name = "hdr_uuid",		.type = "TEXT" },
	{ .name = "hdr_flags",		.type = "INTEGER" },
	{ .name = "hdr_handle",		.type = "INTEGER" },
	{ .name = "hdr_related_handle",	.type = "INTEGER" },
	{ .name = "hdr_ts",		.type = "TEXT" },
	{ .name = "hdr_length",		.type = "INTEGER" },
	{ .name = "hdr_maint_op_class",	.type = "INTEGER" },
	{ .name = "hdr_maint_op_sub_class",	.type = "INTEGER" },
	{ .name = "dpa",		.type = "INTEGER" },
	{ .name = "dpa_flags",		.type = "INTEGER" },
	{ .name = "descriptor",		.type = "INTEGER" },
	{ .name = "type",		.type = "INTEGER" },
	{ .name = "transaction_type",	.type = "INTEGER" },
	{ .name = "channel",		.type = "INTEGER" },
	{ .name = "rank",		.type = "INTEGER" },
	{ .name = "nibble_mask",	.type = "INTEGER" },
	{ .name = "bank_group",		.type = "INTEGER" },
	{ .name = "bank",		.type = "INTEGER" },
	{ .name = "row",		.type = "INTEGER" },
	{ .name = "column",		.type = "INTEGER" },
	{ .name = "cor_mask",		.type = "BLOB" },
	{ .name = "hpa",		.type = "INTEGER" },
	{ .name = "region",		.type = "TEXT" },
	{ .name = "region_uuid",	.type = "TEXT" },
	{ .name = "comp_id",		.type = "BLOB" },
	{ .name = "pldm_entity_id",	.type = "BLOB" },
	{ .name = "pldm_resource_id",	.type = "BLOB" },
	{ .name = "sub_type",		.type = "INTEGER" },
	{ .name = "sub_channel",	.type = "INTEGER" },
	{ .name = "cme_threshold_ev_flags",	.type = "INTEGER" },
	{ .name = "cvme_count",		.type = "INTEGER" },
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

	idx = ras_store_cxl_common_hdr(priv->stmt_cxl_dram_event, &ev->hdr);
	if (idx <= 0)
		return -1;
	sqlite3_bind_int64(priv->stmt_cxl_dram_event, idx++, ev->dpa);
	sqlite3_bind_int(priv->stmt_cxl_dram_event, idx++, ev->dpa_flags);
	sqlite3_bind_int(priv->stmt_cxl_dram_event, idx++, ev->descriptor);
	sqlite3_bind_int(priv->stmt_cxl_dram_event, idx++, ev->type);
	sqlite3_bind_int(priv->stmt_cxl_dram_event, idx++, ev->transaction_type);
	sqlite3_bind_int(priv->stmt_cxl_dram_event, idx++, ev->channel);
	sqlite3_bind_int(priv->stmt_cxl_dram_event, idx++, ev->rank);
	sqlite3_bind_int(priv->stmt_cxl_dram_event, idx++, ev->nibble_mask);
	sqlite3_bind_int(priv->stmt_cxl_dram_event, idx++, ev->bank_group);
	sqlite3_bind_int(priv->stmt_cxl_dram_event, idx++, ev->bank);
	sqlite3_bind_int(priv->stmt_cxl_dram_event, idx++, ev->row);
	sqlite3_bind_int(priv->stmt_cxl_dram_event, idx++, ev->column);
	sqlite3_bind_blob(priv->stmt_cxl_dram_event, idx++, ev->cor_mask,
			  CXL_EVENT_DER_CORRECTION_MASK_SIZE, NULL);
	sqlite3_bind_int64(priv->stmt_cxl_dram_event, idx++, ev->hpa);
	sqlite3_bind_text(priv->stmt_cxl_dram_event, idx++, ev->region, -1, NULL);
	sqlite3_bind_text(priv->stmt_cxl_dram_event, idx++, ev->region_uuid, -1, NULL);
	sqlite3_bind_blob(priv->stmt_cxl_dram_event, idx++, ev->comp_id,
			  CXL_EVENT_GEN_MED_COMP_ID_SIZE, NULL);
	sqlite3_bind_blob(priv->stmt_cxl_dram_event, idx++, ev->entity_id,
			  CXL_PLDM_ENTITY_ID_LEN, NULL);
	sqlite3_bind_blob(priv->stmt_cxl_dram_event, idx++, ev->res_id,
			  CXL_PLDM_RES_ID_LEN, NULL);
	sqlite3_bind_int(priv->stmt_cxl_dram_event, idx++, ev->sub_type);
	sqlite3_bind_int(priv->stmt_cxl_dram_event, idx++, ev->sub_channel);
	sqlite3_bind_int(priv->stmt_cxl_dram_event, idx++,
			 ev->cme_threshold_ev_flags);
	sqlite3_bind_int(priv->stmt_cxl_dram_event, idx++, ev->cvme_count);

	rc = sqlite3_step(priv->stmt_cxl_dram_event);
	if (rc != SQLITE_OK && rc != SQLITE_DONE)
		log(TERM, LOG_ERR,
		    "Failed to do stmt_cxl_dram_event step on sqlite: error = %d\n", rc);
	rc = sqlite3_reset(priv->stmt_cxl_dram_event);
	if (rc != SQLITE_OK && rc != SQLITE_DONE)
		log(TERM, LOG_ERR,
		    "Failed reset stmt_cxl_dram_event on sqlite: error = %d\n", rc);
	log(TERM, LOG_INFO, "register inserted at db\n");

	return rc;
}

/*
 * Table and functions to handle cxl:cxl_memory_module_event
 */
static const struct db_fields cxl_memory_module_event_fields[] = {
	{ .name = "id",			.type = "INTEGER PRIMARY KEY" },
	{ .name = "timestamp",		.type = "TEXT" },
	{ .name = "memdev",		.type = "TEXT" },
	{ .name = "host",		.type = "TEXT" },
	{ .name = "serial",		.type = "INTEGER" },
	{ .name = "log_type",		.type = "TEXT" },
	{ .name = "hdr_uuid",		.type = "TEXT" },
	{ .name = "hdr_flags",		.type = "INTEGER" },
	{ .name = "hdr_handle",		.type = "INTEGER" },
	{ .name = "hdr_related_handle",	.type = "INTEGER" },
	{ .name = "hdr_ts",		.type = "TEXT" },
	{ .name = "hdr_length",		.type = "INTEGER" },
	{ .name = "hdr_maint_op_class",	.type = "INTEGER" },
	{ .name = "hdr_maint_op_sub_class",	.type = "INTEGER" },
	{ .name = "event_type",		.type = "INTEGER" },
	{ .name = "health_status",	.type = "INTEGER" },
	{ .name = "media_status",	.type = "INTEGER" },
	{ .name = "life_used",		.type = "INTEGER" },
	{ .name = "dirty_shutdown_cnt",	.type = "INTEGER" },
	{ .name = "cor_vol_err_cnt",	.type = "INTEGER" },
	{ .name = "cor_per_err_cnt",	.type = "INTEGER" },
	{ .name = "device_temp",	.type = "INTEGER" },
	{ .name = "add_status",		.type = "INTEGER" },
	{ .name = "event_sub_type",	.type = "INTEGER" },
	{ .name = "comp_id",		.type = "BLOB" },
	{ .name = "pldm_entity_id",	.type = "BLOB" },
	{ .name = "pldm_resource_id",	.type = "BLOB" },
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

	idx = ras_store_cxl_common_hdr(priv->stmt_cxl_memory_module_event, &ev->hdr);
	if (idx <= 0)
		return -1;
	sqlite3_bind_int(priv->stmt_cxl_memory_module_event, idx++, ev->event_type);
	sqlite3_bind_int(priv->stmt_cxl_memory_module_event, idx++, ev->health_status);
	sqlite3_bind_int(priv->stmt_cxl_memory_module_event, idx++, ev->media_status);
	sqlite3_bind_int(priv->stmt_cxl_memory_module_event, idx++, ev->life_used);
	sqlite3_bind_int(priv->stmt_cxl_memory_module_event, idx++, ev->dirty_shutdown_cnt);
	sqlite3_bind_int(priv->stmt_cxl_memory_module_event, idx++, ev->cor_vol_err_cnt);
	sqlite3_bind_int(priv->stmt_cxl_memory_module_event, idx++, ev->cor_per_err_cnt);
	sqlite3_bind_int(priv->stmt_cxl_memory_module_event, idx++, ev->device_temp);
	sqlite3_bind_int(priv->stmt_cxl_memory_module_event, idx++, ev->add_status);
	sqlite3_bind_int(priv->stmt_cxl_memory_module_event, idx++, ev->event_sub_type);
	sqlite3_bind_blob(priv->stmt_cxl_memory_module_event, idx++, ev->comp_id,
			  CXL_EVENT_GEN_MED_COMP_ID_SIZE, NULL);
	sqlite3_bind_blob(priv->stmt_cxl_memory_module_event, idx++, ev->entity_id,
			  CXL_PLDM_ENTITY_ID_LEN, NULL);
	sqlite3_bind_blob(priv->stmt_cxl_memory_module_event, idx++, ev->res_id,
			  CXL_PLDM_RES_ID_LEN, NULL);

	rc = sqlite3_step(priv->stmt_cxl_memory_module_event);
	if (rc != SQLITE_OK && rc != SQLITE_DONE)
		log(TERM, LOG_ERR,
		    "Failed to do stmt_cxl_memory_module_event step on sqlite: error = %d\n", rc);
	rc = sqlite3_reset(priv->stmt_cxl_memory_module_event);
	if (rc != SQLITE_OK && rc != SQLITE_DONE)
		log(TERM, LOG_ERR,
		    "Failed reset stmt_cxl_memory_module_event on sqlite: error = %d\n", rc);
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
