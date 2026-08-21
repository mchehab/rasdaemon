// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * Copyright (C) 2013 Mauro Carvalho Chehab <mchehab+huawei@kernel.org>
 * Copyright (c) 2016, The Linux Foundation. All rights reserved.
 */

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

#ifdef HAVE_DB

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

int db_mc_event(struct ras_events *ras, struct ras_mc_event *ev)
{
	struct ras_record_priv *priv = ras->db_priv;
	int rc, pos = 1;

	if (!priv || !priv->stmt_mc_event)
		return 0;
	log(TERM, LOG_INFO, "mc_event store: %p\n", priv->stmt_mc_event);

	db_bind(&mc_event_tab, priv->stmt_mc_event, pos++, (uint64_t)ev->timestamp, -1);
	db_bind(&mc_event_tab, priv->stmt_mc_event, pos++, ev->error_count, -1);
	db_bind(&mc_event_tab, priv->stmt_mc_event, pos++, (uint64_t)ev->error_type, -1);
	db_bind(&mc_event_tab, priv->stmt_mc_event, pos++, (uint64_t)ev->msg, -1);
	db_bind(&mc_event_tab, priv->stmt_mc_event, pos++, (uint64_t)ev->label, -1);
	db_bind(&mc_event_tab, priv->stmt_mc_event, pos++, ev->mc_index, -1);
	db_bind(&mc_event_tab, priv->stmt_mc_event, pos++, ev->top_layer, -1);
	db_bind(&mc_event_tab, priv->stmt_mc_event, pos++, ev->middle_layer, -1);
	db_bind(&mc_event_tab, priv->stmt_mc_event, pos++, ev->lower_layer, -1);
	db_bind(&mc_event_tab, priv->stmt_mc_event, pos++, ev->address, -1);
	db_bind(&mc_event_tab, priv->stmt_mc_event, pos++, ev->grain, -1);
	db_bind(&mc_event_tab, priv->stmt_mc_event, pos++, ev->syndrome, -1);
	db_bind(&mc_event_tab, priv->stmt_mc_event, pos++, (uint64_t)ev->driver_detail, -1);

	rc = db_eval_stmt(priv->stmt_mc_event, "mc_event");
	if (!rc)
		log(TERM, LOG_INFO, "register inserted at db\n");

	return rc;
}

/*
 * Table and functions to handle ras:aer
 */

#ifdef HAVE_AER
static const struct db_fields aer_event_fields[] = {
	{ .name = "id",			.type = DB_TYPE_SERIAL, .is_pk = true },
	{ .name = "timestamp",		.type = DB_TYPE_TIMESTAMP },
	{ .name = "dev_name",		.type = DB_TYPE_TEXT },
	{ .name = "err_type",		.type = DB_TYPE_TEXT },
	{ .name = "err_msg",		.type = DB_TYPE_TEXT },
};

static const struct db_table_descriptor aer_event_tab = {
	.name = "aer_event",
	.fields = aer_event_fields,
	.num_fields = ARRAY_SIZE(aer_event_fields),
};

int db_aer_event(struct ras_events *ras, struct ras_aer_event *ev)
{
	struct ras_record_priv *priv = ras->db_priv;
	int rc, pos = 1;

	if (!priv || !priv->stmt_aer_event)
		return 0;
	log(TERM, LOG_INFO, "aer_event store: %p\n", priv->stmt_aer_event);

	db_bind(&aer_event_tab, priv->stmt_aer_event, pos++, (uint64_t)ev->timestamp, -1);
	db_bind(&aer_event_tab, priv->stmt_aer_event, pos++, (uint64_t)ev->dev_name, -1);
	db_bind(&aer_event_tab, priv->stmt_aer_event, pos++, (uint64_t)ev->error_type, -1);
	db_bind(&aer_event_tab, priv->stmt_aer_event, pos++, (uint64_t)ev->msg, -1);

	rc = db_eval_stmt(priv->stmt_aer_event, "aer_event");
	if (!rc)
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
		{ .name = "timestamp",		.type = DB_TYPE_TIMESTAMP },
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

int db_non_standard_record(struct ras_events *ras, struct ras_non_standard_event *ev)
{
	struct ras_record_priv *priv = ras->db_priv;
	int rc, pos = 1;

	if (!priv || !priv->stmt_non_standard_record)
		return 0;
	log(TERM, LOG_INFO, "non_standard_event store: %p\n", priv->stmt_non_standard_record);

	db_bind(&non_standard_event_tab, priv->stmt_non_standard_record, pos++, (uint64_t)ev->timestamp, -1);
	db_bind(&non_standard_event_tab, priv->stmt_non_standard_record, pos++, (uint64_t)ev->sec_type, -1);
	db_bind(&non_standard_event_tab, priv->stmt_non_standard_record, pos++, (uint64_t)ev->fru_id,  16);
	db_bind(&non_standard_event_tab, priv->stmt_non_standard_record, pos++, (uint64_t)ev->fru_text, -1);
	db_bind(&non_standard_event_tab, priv->stmt_non_standard_record, pos++, (uint64_t)ev->severity, -1);
	db_bind(&non_standard_event_tab, priv->stmt_non_standard_record, pos++, (uint64_t)ev->error,  ev->length);

	rc = db_eval_stmt(priv->stmt_non_standard_record, "non_standard_record");
	if (!rc)
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
		{ .name = "timestamp",		.type = DB_TYPE_TIMESTAMP },
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

int db_arm_record(struct ras_events *ras, struct ras_arm_event *ev)
{
	struct ras_record_priv *priv = ras->db_priv;
	int rc, pos = 1;

	if (!priv || !priv->stmt_arm_record)
		return 0;
	log(TERM, LOG_INFO, "arm_event store: %p\n", priv->stmt_arm_record);

	db_bind(&arm_event_tab, priv->stmt_arm_record, pos++, (uint64_t)ev->timestamp, -1);
	db_bind(&arm_event_tab, priv->stmt_arm_record, pos++, ev->error_count, -1);
	db_bind(&arm_event_tab, priv->stmt_arm_record, pos++, ev->affinity, -1);
	db_bind(&arm_event_tab, priv->stmt_arm_record, pos++, ev->mpidr, -1);
	db_bind(&arm_event_tab, priv->stmt_arm_record, pos++, ev->running_state, -1);
	db_bind(&arm_event_tab, priv->stmt_arm_record, pos++, ev->psci_state, -1);
	db_bind(&arm_event_tab, priv->stmt_arm_record, pos++, (uint64_t)ev->pei_error,  ev->pei_len);
	db_bind(&arm_event_tab, priv->stmt_arm_record, pos++, (uint64_t)ev->ctx_error,  ev->ctx_len);
	db_bind(&arm_event_tab, priv->stmt_arm_record, pos++, (uint64_t)ev->vsei_error,  ev->oem_len);
	db_bind(&arm_event_tab, priv->stmt_arm_record, pos++, (uint64_t)ev->error_types, -1);
	db_bind(&arm_event_tab, priv->stmt_arm_record, pos++, (uint64_t)ev->error_flags, -1);
	db_bind(&arm_event_tab, priv->stmt_arm_record, pos++, ev->error_info, -1);
	db_bind(&arm_event_tab, priv->stmt_arm_record, pos++, ev->virt_fault_addr, -1);
	db_bind(&arm_event_tab, priv->stmt_arm_record, pos++, ev->phy_fault_addr, -1);

	rc = db_eval_stmt(priv->stmt_arm_record, "arm_record");
	if (!rc)
		log(TERM, LOG_INFO, "register inserted at db\n");

	return rc;
}
#endif

#ifdef HAVE_EXTLOG
static const struct db_fields extlog_event_fields[] = {
	{ .name = "id",			.type = DB_TYPE_SERIAL, .is_pk = true },
	{ .name = "timestamp",		.type = DB_TYPE_TIMESTAMP },
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

int db_extlog_mem_record(struct ras_events *ras, struct ras_extlog_event *ev)
{
	struct ras_record_priv *priv = ras->db_priv;
	int rc, pos = 1;

	if (!priv || !priv->stmt_extlog_record)
		return 0;
	log(TERM, LOG_INFO, "extlog_record store: %p\n", priv->stmt_extlog_record);

	db_bind(&extlog_event_tab, priv->stmt_extlog_record, pos++, (uint64_t)ev->timestamp, -1);
	db_bind(&extlog_event_tab, priv->stmt_extlog_record, pos++, ev->etype, -1);
	db_bind(&extlog_event_tab, priv->stmt_extlog_record, pos++, ev->error_seq, -1);
	db_bind(&extlog_event_tab, priv->stmt_extlog_record, pos++, ev->severity, -1);
	db_bind(&extlog_event_tab, priv->stmt_extlog_record, pos++, ev->address, -1);
	db_bind(&extlog_event_tab, priv->stmt_extlog_record, pos++, (uint64_t)ev->fru_id,  16);
	db_bind(&extlog_event_tab, priv->stmt_extlog_record, pos++, (uint64_t)ev->fru_text, -1);
	db_bind(&extlog_event_tab, priv->stmt_extlog_record, pos++, (uint64_t)ev->cper_data,  ev->cper_data_length);

	rc = db_eval_stmt(priv->stmt_extlog_record, "extlog_record");
	if (!rc)
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
	{ .name = "timestamp",		.type = DB_TYPE_TIMESTAMP },

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

int db_mce_record(struct ras_events *ras, struct mce_event *ev)
{
	struct ras_record_priv *priv = ras->db_priv;
	int rc, pos = 1;

	if (!priv || !priv->stmt_mce_record)
		return 0;
	log(TERM, LOG_INFO, "mce_record store: %p\n", priv->stmt_mce_record);

	db_bind(&mce_record_tab, priv->stmt_mce_record, pos++, (uint64_t)ev->timestamp, -1);
	db_bind(&mce_record_tab, priv->stmt_mce_record, pos++, ev->mcgcap, -1);
	db_bind(&mce_record_tab, priv->stmt_mce_record, pos++, ev->mcgstatus, -1);
	db_bind(&mce_record_tab, priv->stmt_mce_record, pos++, ev->status, -1);
	db_bind(&mce_record_tab, priv->stmt_mce_record, pos++, ev->addr, -1);
	db_bind(&mce_record_tab, priv->stmt_mce_record, pos++, ev->misc, -1);
	db_bind(&mce_record_tab, priv->stmt_mce_record, pos++, ev->ip, -1);
	db_bind(&mce_record_tab, priv->stmt_mce_record, pos++, ev->tsc, -1);
	db_bind(&mce_record_tab, priv->stmt_mce_record, pos++, ev->walltime, -1);
	db_bind(&mce_record_tab, priv->stmt_mce_record, pos++, ev->ppin, -1);
	db_bind(&mce_record_tab, priv->stmt_mce_record, pos++, ev->cpu, -1);
	db_bind(&mce_record_tab, priv->stmt_mce_record, pos++, ev->cpuid, -1);
	db_bind(&mce_record_tab, priv->stmt_mce_record, pos++, ev->apicid, -1);
	db_bind(&mce_record_tab, priv->stmt_mce_record, pos++, ev->socketid, -1);
	db_bind(&mce_record_tab, priv->stmt_mce_record, pos++, ev->cs, -1);
	db_bind(&mce_record_tab, priv->stmt_mce_record, pos++, ev->bank, -1);
	db_bind(&mce_record_tab, priv->stmt_mce_record, pos++, ev->cpuvendor, -1);
	db_bind(&mce_record_tab, priv->stmt_mce_record, pos++, ev->microcode, -1);

	db_bind(&mce_record_tab, priv->stmt_mce_record, pos++, (uint64_t)ev->bank_name, -1);
	db_bind(&mce_record_tab, priv->stmt_mce_record, pos++, (uint64_t)ev->error_msg, -1);
	db_bind(&mce_record_tab, priv->stmt_mce_record, pos++, (uint64_t)ev->mcgstatus_msg, -1);
	db_bind(&mce_record_tab, priv->stmt_mce_record, pos++, (uint64_t)ev->mcistatus_msg, -1);
	db_bind(&mce_record_tab, priv->stmt_mce_record, pos++, (uint64_t)ev->mcastatus_msg, -1);
	db_bind(&mce_record_tab, priv->stmt_mce_record, pos++, (uint64_t)ev->user_action, -1);
	db_bind(&mce_record_tab, priv->stmt_mce_record, pos++, (uint64_t)ev->mc_location, -1);

	rc = db_eval_stmt(priv->stmt_mce_record, "mce_record");
	if (!rc)
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
	{ .name = "timestamp",		.type = DB_TYPE_TIMESTAMP },
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

int db_devlink_event(struct ras_events *ras, struct devlink_event *ev)
{
	struct ras_record_priv *priv = ras->db_priv;
	int rc, pos = 1;

	if (!priv || !priv->stmt_devlink_event)
		return 0;
	log(TERM, LOG_INFO, "devlink_event store: %p\n", priv->stmt_devlink_event);

	db_bind(&devlink_event_tab, priv->stmt_devlink_event, pos++, (uint64_t)ev->timestamp, -1);
	db_bind(&devlink_event_tab, priv->stmt_devlink_event, pos++, (uint64_t)ev->bus_name, -1);
	db_bind(&devlink_event_tab, priv->stmt_devlink_event, pos++, (uint64_t)ev->dev_name, -1);
	db_bind(&devlink_event_tab, priv->stmt_devlink_event, pos++, (uint64_t)ev->driver_name, -1);
	db_bind(&devlink_event_tab, priv->stmt_devlink_event, pos++, (uint64_t)ev->reporter_name, -1);
	db_bind(&devlink_event_tab, priv->stmt_devlink_event, pos++, (uint64_t)ev->msg, -1);

	rc = db_eval_stmt(priv->stmt_devlink_event, "devlink_event");
	if (!rc)
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
	{ .name = "timestamp",		.type = DB_TYPE_TIMESTAMP },
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

int db_diskerror_event(struct ras_events *ras, struct diskerror_event *ev)
{
	struct ras_record_priv *priv = ras->db_priv;
	int rc, pos = 1;

	if (!priv || !priv->stmt_diskerror_event)
		return 0;
	log(TERM, LOG_INFO, "diskerror_event store: %p\n", priv->stmt_diskerror_event);

	db_bind(&diskerror_event_tab, priv->stmt_diskerror_event, pos++, (uint64_t)ev->timestamp, -1);
	db_bind(&diskerror_event_tab, priv->stmt_diskerror_event, pos++, (uint64_t)ev->dev, -1);
	db_bind(&diskerror_event_tab, priv->stmt_diskerror_event, pos++, ev->sector, -1);
	db_bind(&diskerror_event_tab, priv->stmt_diskerror_event, pos++, ev->nr_sector, -1);
	db_bind(&diskerror_event_tab, priv->stmt_diskerror_event, pos++, (uint64_t)ev->error, -1);
	db_bind(&diskerror_event_tab, priv->stmt_diskerror_event, pos++, (uint64_t)ev->rwbs, -1);
	db_bind(&diskerror_event_tab, priv->stmt_diskerror_event, pos++, (uint64_t)ev->cmd, -1);

	rc = db_eval_stmt(priv->stmt_diskerror_event, "diskerror_event");
	if (!rc)
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
	{ .name = "timestamp",		.type = DB_TYPE_TIMESTAMP },
	{ .name = "pfn",		.type = DB_TYPE_TEXT },
	{ .name = "page_type",		.type = DB_TYPE_TEXT },
	{ .name = "action_result",	.type = DB_TYPE_TEXT },
};

static const struct db_table_descriptor mf_event_tab = {
	.name = "memory_failure_event",
	.fields = mf_event_fields,
	.num_fields = ARRAY_SIZE(mf_event_fields),
};

int db_mf_event(struct ras_events *ras, struct ras_mf_event *ev)
{
	struct ras_record_priv *priv = ras->db_priv;
	int rc, pos = 1;

	if (!priv || !priv->stmt_mf_event)
		return 0;
	log(TERM, LOG_INFO, "memory_failure_event store: %p\n", priv->stmt_mf_event);

	db_bind(&mf_event_tab, priv->stmt_mf_event, pos++, (uint64_t)ev->timestamp, -1);
	db_bind(&mf_event_tab, priv->stmt_mf_event, pos++, (uint64_t)ev->pfn, -1);
	db_bind(&mf_event_tab, priv->stmt_mf_event, pos++, (uint64_t)ev->page_type, -1);
	db_bind(&mf_event_tab, priv->stmt_mf_event, pos++, (uint64_t)ev->action_result, -1);

	rc = db_eval_stmt(priv->stmt_mf_event, "mf_event");
	if (!rc)
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
	{ .name = "timestamp",		.type = DB_TYPE_TIMESTAMP },
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

int db_cxl_poison_event(struct ras_events *ras, struct ras_cxl_poison_event *ev)
{
	struct ras_record_priv *priv = ras->db_priv;
	int rc, pos = 1;

	if (!priv || !priv->stmt_cxl_poison_event)
		return 0;
	log(TERM, LOG_INFO, "cxl_poison_event store: %p\n", priv->stmt_cxl_poison_event);

	db_bind(&cxl_poison_event_tab, priv->stmt_cxl_poison_event, pos++, (uint64_t)ev->timestamp, -1);
	db_bind(&cxl_poison_event_tab, priv->stmt_cxl_poison_event, pos++, (uint64_t)ev->memdev, -1);
	db_bind(&cxl_poison_event_tab, priv->stmt_cxl_poison_event, pos++, (uint64_t)ev->host, -1);
	db_bind(&cxl_poison_event_tab, priv->stmt_cxl_poison_event, pos++, ev->serial, -1);
	db_bind(&cxl_poison_event_tab, priv->stmt_cxl_poison_event, pos++, (uint64_t)ev->trace_type, -1);
	db_bind(&cxl_poison_event_tab, priv->stmt_cxl_poison_event, pos++, (uint64_t)ev->region, -1);
	db_bind(&cxl_poison_event_tab, priv->stmt_cxl_poison_event, pos++, (uint64_t)ev->uuid, -1);
	db_bind(&cxl_poison_event_tab, priv->stmt_cxl_poison_event, pos++, ev->hpa, -1);
	db_bind(&cxl_poison_event_tab, priv->stmt_cxl_poison_event, pos++, ev->dpa, -1);
	db_bind(&cxl_poison_event_tab, priv->stmt_cxl_poison_event, pos++, ev->dpa_length, -1);
	db_bind(&cxl_poison_event_tab, priv->stmt_cxl_poison_event, pos++, (uint64_t)ev->source, -1);
	db_bind(&cxl_poison_event_tab, priv->stmt_cxl_poison_event, pos++, ev->flags, -1);
	db_bind(&cxl_poison_event_tab, priv->stmt_cxl_poison_event, pos++, (uint64_t)ev->overflow_ts, -1);
	db_bind(&cxl_poison_event_tab, priv->stmt_cxl_poison_event, pos++, ev->hpa_alias0, -1);

	rc = db_eval_stmt(priv->stmt_cxl_poison_event, "cxl_poison_event");
	if (!rc)
		log(TERM, LOG_INFO, "register inserted at db\n");

	return rc;
}

/*
 * Table and functions to handle cxl:cxl_aer_uncorrectable_error
 */
static const struct db_fields cxl_aer_ue_event_fields[] = {
	{ .name = "id",			.type = DB_TYPE_SERIAL, .is_pk = true },
	{ .name = "timestamp",		.type = DB_TYPE_TIMESTAMP },
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

int db_cxl_aer_ue_event(struct ras_events *ras, struct ras_cxl_aer_ue_event *ev)
{
	struct ras_record_priv *priv = ras->db_priv;
	int rc, pos = 1;

	if (!priv || !priv->stmt_cxl_aer_ue_event)
		return 0;
	log(TERM, LOG_INFO, "cxl_aer_ue_event store: %p\n", priv->stmt_cxl_aer_ue_event);

	db_bind(&cxl_aer_ue_event_tab, priv->stmt_cxl_aer_ue_event, pos++, (uint64_t)ev->timestamp, -1);
	db_bind(&cxl_aer_ue_event_tab, priv->stmt_cxl_aer_ue_event, pos++, (uint64_t)ev->memdev, -1);
	db_bind(&cxl_aer_ue_event_tab, priv->stmt_cxl_aer_ue_event, pos++, (uint64_t)ev->host, -1);
	db_bind(&cxl_aer_ue_event_tab, priv->stmt_cxl_aer_ue_event, pos++, ev->serial, -1);
	db_bind(&cxl_aer_ue_event_tab, priv->stmt_cxl_aer_ue_event, pos++, ev->error_status, -1);
	db_bind(&cxl_aer_ue_event_tab, priv->stmt_cxl_aer_ue_event, pos++, ev->first_error, -1);
	db_bind(&cxl_aer_ue_event_tab, priv->stmt_cxl_aer_ue_event, pos++, (uint64_t)ev->header_log, CXL_HEADERLOG_SIZE);

	rc = db_eval_stmt(priv->stmt_cxl_aer_ue_event, "cxl_aer_ue_event");
	if (!rc)
		log(TERM, LOG_INFO, "register inserted at db\n");

	return rc;
}

/*
 * Table and functions to handle cxl:cxl_aer_correctable_error
 */
static const struct db_fields cxl_aer_ce_event_fields[] = {
	{ .name = "id",			.type = DB_TYPE_SERIAL, .is_pk = true },
	{ .name = "timestamp",		.type = DB_TYPE_TIMESTAMP },
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

int db_cxl_aer_ce_event(struct ras_events *ras, struct ras_cxl_aer_ce_event *ev)
{
	struct ras_record_priv *priv = ras->db_priv;
	int rc, pos = 1;

	if (!priv || !priv->stmt_cxl_aer_ce_event)
		return 0;
	log(TERM, LOG_INFO, "cxl_aer_ce_event store: %p\n", priv->stmt_cxl_aer_ce_event);

	db_bind(&cxl_aer_ce_event_tab, priv->stmt_cxl_aer_ce_event, pos++, (uint64_t)ev->timestamp, -1);
	db_bind(&cxl_aer_ce_event_tab, priv->stmt_cxl_aer_ce_event, pos++, (uint64_t)ev->memdev, -1);
	db_bind(&cxl_aer_ce_event_tab, priv->stmt_cxl_aer_ce_event, pos++, (uint64_t)ev->host, -1);
	db_bind(&cxl_aer_ce_event_tab, priv->stmt_cxl_aer_ce_event, pos++, ev->serial, -1);
	db_bind(&cxl_aer_ce_event_tab, priv->stmt_cxl_aer_ce_event, pos++, ev->error_status, -1);

	rc = db_eval_stmt(priv->stmt_cxl_aer_ce_event, "cxl_aer_ce_event");
	if (!rc)
		log(TERM, LOG_INFO, "register inserted at db\n");

	return rc;
}

/*
 * Table and functions to handle cxl:cxl_overflow
 */
static const struct db_fields cxl_overflow_event_fields[] = {
	{ .name = "id",			.type = DB_TYPE_SERIAL, .is_pk = true },
	{ .name = "timestamp",		.type = DB_TYPE_TIMESTAMP },
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

int db_cxl_overflow_event(struct ras_events *ras, struct ras_cxl_overflow_event *ev)
{
	struct ras_record_priv *priv = ras->db_priv;
	int rc, pos = 1;

	if (!priv || !priv->stmt_cxl_overflow_event)
		return 0;
	log(TERM, LOG_INFO, "cxl_overflow_event store: %p\n", priv->stmt_cxl_overflow_event);

	db_bind(&cxl_overflow_event_tab, priv->stmt_cxl_overflow_event, pos++, (uint64_t)ev->timestamp, -1);
	db_bind(&cxl_overflow_event_tab, priv->stmt_cxl_overflow_event, pos++, (uint64_t)ev->memdev, -1);
	db_bind(&cxl_overflow_event_tab, priv->stmt_cxl_overflow_event, pos++, (uint64_t)ev->host, -1);
	db_bind(&cxl_overflow_event_tab, priv->stmt_cxl_overflow_event, pos++, ev->serial, -1);
	db_bind(&cxl_overflow_event_tab, priv->stmt_cxl_overflow_event, pos++, (uint64_t)ev->log_type, -1);
	db_bind(&cxl_overflow_event_tab, priv->stmt_cxl_overflow_event, pos++, ev->count, -1);
	db_bind(&cxl_overflow_event_tab, priv->stmt_cxl_overflow_event, pos++, (uint64_t)ev->first_ts, -1);
	db_bind(&cxl_overflow_event_tab, priv->stmt_cxl_overflow_event, pos++, (uint64_t)ev->last_ts, -1);

	rc = db_eval_stmt(priv->stmt_cxl_overflow_event, "cxl_overflow_event");
	if (!rc)
		log(TERM, LOG_INFO, "register inserted at db\n");

	return rc;
}

static int db_cxl_common_hdr(const struct db_table_descriptor *db_tab,
			     struct ras_stmt *stmt,
			     struct ras_cxl_event_common_hdr *hdr)
{
	int idx = 1;

	if (!stmt || !hdr)
		return -1;

	db_bind(db_tab, stmt, idx++, (uint64_t)hdr->timestamp, -1);
	db_bind(db_tab, stmt, idx++, (uint64_t)hdr->memdev, -1);
	db_bind(db_tab, stmt, idx++, (uint64_t)hdr->host, -1);
	db_bind(db_tab, stmt, idx++, hdr->serial, -1);
	db_bind(db_tab, stmt, idx++, (uint64_t)hdr->log_type, -1);
	db_bind(db_tab, stmt, idx++, (uint64_t)hdr->hdr_uuid, -1);
	db_bind(db_tab, stmt, idx++, hdr->hdr_flags, -1);
	db_bind(db_tab, stmt, idx++, hdr->hdr_handle, -1);
	db_bind(db_tab, stmt, idx++, hdr->hdr_related_handle, -1);
	db_bind(db_tab, stmt, idx++, (uint64_t)hdr->hdr_timestamp, -1);
	db_bind(db_tab, stmt, idx++, hdr->hdr_length, -1);
	db_bind(db_tab, stmt, idx++, hdr->hdr_maint_op_class, -1);
	db_bind(db_tab, stmt, idx++, hdr->hdr_maint_op_sub_class, -1);
	db_bind(db_tab, stmt, idx++, hdr->hdr_ld_id, -1);
	db_bind(db_tab, stmt, idx++, hdr->hdr_head_id, -1);

	return idx;
}

/*
 * Table and functions to handle cxl:cxl_generic_event
 */
static const struct db_fields cxl_generic_event_fields[] = {
	{ .name = "id",				.type = DB_TYPE_SERIAL, .is_pk = true },
	{ .name = "timestamp",			.type = DB_TYPE_TIMESTAMP },
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

int db_cxl_generic_event(struct ras_events *ras, struct ras_cxl_generic_event *ev)
{
	struct ras_record_priv *priv = ras->db_priv;
	int idx;
	int rc;

	if (!priv || !priv->stmt_cxl_generic_event)
		return -1;
	log(TERM, LOG_INFO, "cxl_generic_event store: %p\n", priv->stmt_cxl_generic_event);

	idx = db_cxl_common_hdr(&cxl_generic_event_tab,
				priv->stmt_cxl_generic_event, &ev->hdr);
	if (idx <= 0)
		return -1;

	db_bind(&cxl_generic_event_tab, priv->stmt_cxl_generic_event,
		idx++, (uint64_t)ev->data, CXL_EVENT_RECORD_DATA_LENGTH);

	rc = db_eval_stmt(priv->stmt_cxl_generic_event, "cxl_generic_event");
	if (!rc)
		log(TERM, LOG_INFO, "register inserted at db\n");

	return rc;
}

/*
 * Table and functions to handle cxl:cxl_general_media_event
 */
static const struct db_fields cxl_general_media_event_fields[] = {
	{ .name = "id",				.type = DB_TYPE_SERIAL, .is_pk = true },
	{ .name = "timestamp",			.type = DB_TYPE_TIMESTAMP },
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

int db_cxl_general_media_event(struct ras_events *ras,
				      struct ras_cxl_general_media_event *ev)
{
	struct ras_record_priv *priv = ras->db_priv;
	int idx;
	int rc;

	if (!priv || !priv->stmt_cxl_general_media_event)
		return -1;
	log(TERM, LOG_INFO, "cxl_general_media_event store: %p\n",
	    priv->stmt_cxl_general_media_event);

	idx = db_cxl_common_hdr(&cxl_general_media_event_tab,
				priv->stmt_cxl_general_media_event, &ev->hdr);
	if (idx <= 0)
		return -1;
	db_bind(&cxl_general_media_event_tab, priv->stmt_cxl_general_media_event, idx++, ev->dpa, -1);
	db_bind(&cxl_general_media_event_tab, priv->stmt_cxl_general_media_event, idx++, ev->dpa_flags, -1);
	db_bind(&cxl_general_media_event_tab, priv->stmt_cxl_general_media_event, idx++, ev->descriptor, -1);
	db_bind(&cxl_general_media_event_tab, priv->stmt_cxl_general_media_event, idx++, ev->type, -1);
	db_bind(&cxl_general_media_event_tab, priv->stmt_cxl_general_media_event, idx++, ev->transaction_type, -1);
	db_bind(&cxl_general_media_event_tab, priv->stmt_cxl_general_media_event, idx++, ev->channel, -1);
	db_bind(&cxl_general_media_event_tab, priv->stmt_cxl_general_media_event, idx++, ev->rank, -1);
	db_bind(&cxl_general_media_event_tab, priv->stmt_cxl_general_media_event, idx++, ev->device, -1);
	db_bind(&cxl_general_media_event_tab, priv->stmt_cxl_general_media_event, idx++, (uint64_t)ev->comp_id, CXL_EVENT_GEN_MED_COMP_ID_SIZE);
	db_bind(&cxl_general_media_event_tab, priv->stmt_cxl_general_media_event, idx++, ev->hpa, -1);
	db_bind(&cxl_general_media_event_tab, priv->stmt_cxl_general_media_event, idx++, (uint64_t)ev->region, -1);
	db_bind(&cxl_general_media_event_tab, priv->stmt_cxl_general_media_event, idx++, (uint64_t)ev->region_uuid, -1);
	db_bind(&cxl_general_media_event_tab, priv->stmt_cxl_general_media_event, idx++, (uint64_t)ev->entity_id, CXL_PLDM_ENTITY_ID_LEN);
	db_bind(&cxl_general_media_event_tab, priv->stmt_cxl_general_media_event, idx++, (uint64_t)ev->res_id, CXL_PLDM_RES_ID_LEN);
	db_bind(&cxl_general_media_event_tab, priv->stmt_cxl_general_media_event, idx++, ev->sub_type, -1);
	db_bind(&cxl_general_media_event_tab, priv->stmt_cxl_general_media_event, idx++, ev->cme_threshold_ev_flags, -1);
	db_bind(&cxl_general_media_event_tab, priv->stmt_cxl_general_media_event, idx++, ev->cme_count, -1);
	db_bind(&cxl_general_media_event_tab, priv->stmt_cxl_general_media_event, idx++, ev->hpa_alias0, -1);

	rc = db_eval_stmt(priv->stmt_cxl_general_media_event, "cxl_general_media_event");
	if (!rc)
		log(TERM, LOG_INFO, "register inserted at db\n");

	return rc;
}

/*
 * Table and functions to handle cxl:cxl_dram_event
 */
static const struct db_fields cxl_dram_event_fields[] = {
	{ .name = "id",				.type = DB_TYPE_SERIAL, .is_pk = true },
	{ .name = "timestamp",			.type = DB_TYPE_TIMESTAMP },
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

int db_cxl_dram_event(struct ras_events *ras, struct ras_cxl_dram_event *ev)
{
	struct ras_record_priv *priv = ras->db_priv;
	int idx;
	int rc;

	if (!priv || !priv->stmt_cxl_dram_event)
		return -1;
	log(TERM, LOG_INFO, "cxl_dram_event store: %p\n",
	    priv->stmt_cxl_dram_event);

	idx = db_cxl_common_hdr(&cxl_dram_event_tab,
				priv->stmt_cxl_dram_event, &ev->hdr);
	if (idx <= 0)
		return -1;

	db_bind(&cxl_dram_event_tab, priv->stmt_cxl_dram_event, idx++, ev->dpa, -1);
	db_bind(&cxl_dram_event_tab, priv->stmt_cxl_dram_event, idx++, ev->dpa_flags, -1);
	db_bind(&cxl_dram_event_tab, priv->stmt_cxl_dram_event, idx++, ev->descriptor, -1);
	db_bind(&cxl_dram_event_tab, priv->stmt_cxl_dram_event, idx++, ev->type, -1);
	db_bind(&cxl_dram_event_tab, priv->stmt_cxl_dram_event, idx++, ev->transaction_type, -1);
	db_bind(&cxl_dram_event_tab, priv->stmt_cxl_dram_event, idx++, ev->channel, -1);
	db_bind(&cxl_dram_event_tab, priv->stmt_cxl_dram_event, idx++, ev->rank, -1);
	db_bind(&cxl_dram_event_tab, priv->stmt_cxl_dram_event, idx++, ev->nibble_mask, -1);
	db_bind(&cxl_dram_event_tab, priv->stmt_cxl_dram_event, idx++, ev->bank_group, -1);
	db_bind(&cxl_dram_event_tab, priv->stmt_cxl_dram_event, idx++, ev->bank, -1);
	db_bind(&cxl_dram_event_tab, priv->stmt_cxl_dram_event, idx++, ev->row, -1);
	db_bind(&cxl_dram_event_tab, priv->stmt_cxl_dram_event, idx++, ev->column, -1);
	db_bind(&cxl_dram_event_tab, priv->stmt_cxl_dram_event, idx++, (uint64_t)ev->cor_mask, CXL_EVENT_DER_CORRECTION_MASK_SIZE);
	db_bind(&cxl_dram_event_tab, priv->stmt_cxl_dram_event, idx++, ev->hpa, -1);
	db_bind(&cxl_dram_event_tab, priv->stmt_cxl_dram_event, idx++, (uint64_t)ev->region, -1);
	db_bind(&cxl_dram_event_tab, priv->stmt_cxl_dram_event, idx++, (uint64_t)ev->region_uuid, -1);
	db_bind(&cxl_dram_event_tab, priv->stmt_cxl_dram_event, idx++, (uint64_t)ev->comp_id, CXL_EVENT_GEN_MED_COMP_ID_SIZE);
	db_bind(&cxl_dram_event_tab, priv->stmt_cxl_dram_event, idx++, (uint64_t)ev->entity_id, CXL_PLDM_ENTITY_ID_LEN);
	db_bind(&cxl_dram_event_tab, priv->stmt_cxl_dram_event, idx++, (uint64_t)ev->res_id, CXL_PLDM_RES_ID_LEN);
	db_bind(&cxl_dram_event_tab, priv->stmt_cxl_dram_event, idx++, ev->sub_type, -1);
	db_bind(&cxl_dram_event_tab, priv->stmt_cxl_dram_event, idx++, ev->sub_channel, -1);
	db_bind(&cxl_dram_event_tab, priv->stmt_cxl_dram_event, idx++, ev->cme_threshold_ev_flags, -1);
	db_bind(&cxl_dram_event_tab, priv->stmt_cxl_dram_event, idx++, ev->cvme_count, -1);
	db_bind(&cxl_dram_event_tab, priv->stmt_cxl_dram_event, idx++, ev->hpa_alias0, -1);

	rc = db_eval_stmt(priv->stmt_cxl_dram_event, "cxl_dram_event");
	if (!rc)
		log(TERM, LOG_INFO, "register inserted at db\n");

	return rc;
}

/*
 * Table and functions to handle cxl:cxl_memory_module_event
 */
static const struct db_fields cxl_memory_module_event_fields[] = {
	{ .name = "id",				.type = DB_TYPE_SERIAL, .is_pk = true },
	{ .name = "timestamp",			.type = DB_TYPE_TIMESTAMP },
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

int db_cxl_memory_module_event(struct ras_events *ras,
				      struct ras_cxl_memory_module_event *ev)
{
	struct ras_record_priv *priv = ras->db_priv;
	int idx;
	int rc;

	if (!priv || !priv->stmt_cxl_memory_module_event)
		return -1;
	log(TERM, LOG_INFO, "cxl_memory_module_event store: %p\n",
	    priv->stmt_cxl_memory_module_event);

	idx = db_cxl_common_hdr(&cxl_memory_module_event_tab,
				priv->stmt_cxl_memory_module_event, &ev->hdr);
	if (idx <= 0)
		return -1;

	db_bind(&cxl_memory_module_event_tab, priv->stmt_cxl_memory_module_event, idx++, ev->event_type, -1);
	db_bind(&cxl_memory_module_event_tab, priv->stmt_cxl_memory_module_event, idx++, ev->health_status, -1);
	db_bind(&cxl_memory_module_event_tab, priv->stmt_cxl_memory_module_event, idx++, ev->media_status, -1);
	db_bind(&cxl_memory_module_event_tab, priv->stmt_cxl_memory_module_event, idx++, ev->life_used, -1);
	db_bind(&cxl_memory_module_event_tab, priv->stmt_cxl_memory_module_event, idx++, ev->dirty_shutdown_cnt, -1);
	db_bind(&cxl_memory_module_event_tab, priv->stmt_cxl_memory_module_event, idx++, ev->cor_vol_err_cnt, -1);
	db_bind(&cxl_memory_module_event_tab, priv->stmt_cxl_memory_module_event, idx++, ev->cor_per_err_cnt, -1);
	db_bind(&cxl_memory_module_event_tab, priv->stmt_cxl_memory_module_event, idx++, ev->device_temp, -1);
	db_bind(&cxl_memory_module_event_tab, priv->stmt_cxl_memory_module_event, idx++, ev->add_status, -1);
	db_bind(&cxl_memory_module_event_tab, priv->stmt_cxl_memory_module_event, idx++, ev->event_sub_type, -1);
	db_bind(&cxl_memory_module_event_tab, priv->stmt_cxl_memory_module_event, idx++, (uint64_t)ev->comp_id, CXL_EVENT_GEN_MED_COMP_ID_SIZE);
	db_bind(&cxl_memory_module_event_tab, priv->stmt_cxl_memory_module_event, idx++, (uint64_t)ev->entity_id, CXL_PLDM_ENTITY_ID_LEN);
	db_bind(&cxl_memory_module_event_tab, priv->stmt_cxl_memory_module_event, idx++, (uint64_t)ev->res_id, CXL_PLDM_RES_ID_LEN);

	rc = db_eval_stmt(priv->stmt_cxl_memory_module_event, "cxl_memory_module_event");
	if (!rc)
		log(TERM, LOG_INFO, "register inserted at db\n");

	return rc;
}
#endif

#ifdef HAVE_SIGNAL
static const struct db_fields signal_event_fields[] = {
	{ .name = "id",		.type = DB_TYPE_SERIAL, .is_pk = true },
	{ .name = "timestamp",	.type = DB_TYPE_TIMESTAMP },
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

int db_signal_event(struct ras_events *ras, struct ras_signal_event *ev)
{
	int rc, pos = 0;
	struct ras_record_priv *priv = ras->db_priv;

	if (!priv || !priv->stmt_signal_event)
		return -1;
	log(TERM, LOG_INFO, "signal_event store: %p\n", priv->stmt_signal_event);

	db_bind(&signal_event_tab, priv->stmt_signal_event, pos++, (uint64_t)ev->timestamp, -1);
	db_bind(&signal_event_tab, priv->stmt_signal_event, pos++, ev->sig, -1);
	db_bind(&signal_event_tab, priv->stmt_signal_event, pos++, ev->error_no, -1);
	db_bind(&signal_event_tab, priv->stmt_signal_event, pos++, ev->code, -1);
	db_bind(&signal_event_tab, priv->stmt_signal_event, pos++, (uint64_t)ev->comm, -1);
	db_bind(&signal_event_tab, priv->stmt_signal_event, pos++, ev->pid, -1);
	db_bind(&signal_event_tab, priv->stmt_signal_event, pos++, ev->group, -1);
	db_bind(&signal_event_tab, priv->stmt_signal_event, pos++, ev->result, -1);

	rc = db_eval_stmt(priv->stmt_signal_event, "signal_event");
	if (!rc)
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
	{ .name = "timestamp",		.type = DB_TYPE_TIMESTAMP },
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

int db_reri_event(struct ras_events *ras, struct ras_reri_event *ev)
{
	int rc, pos = 1;
	struct ras_record_priv *priv = ras->db_priv;

	if (!priv || !priv->stmt_reri_event)
		return 0;
	log(TERM, LOG_INFO, "reri_event store: %p\n", priv->stmt_reri_event);

	db_bind(&reri_event_tab, priv->stmt_reri_event, pos++, (uint64_t)ev->timestamp, -1);
	db_bind(&reri_event_tab, priv->stmt_reri_event, pos++, ev->err_src_id, -1);
	db_bind(&reri_event_tab, priv->stmt_reri_event, pos++, ev->source_type, -1);
	db_bind(&reri_event_tab, priv->stmt_reri_event, pos++, ev->severity, -1);
	db_bind(&reri_event_tab, priv->stmt_reri_event, pos++, ev->hart_id, -1);
	db_bind(&reri_event_tab, priv->stmt_reri_event, pos++, ev->cluster_id, -1);
	db_bind(&reri_event_tab, priv->stmt_reri_event, pos++, ev->status, -1);
	db_bind(&reri_event_tab, priv->stmt_reri_event, pos++, ev->addr_info, -1);
	db_bind(&reri_event_tab, priv->stmt_reri_event, pos++, ev->info, -1);
	db_bind(&reri_event_tab, priv->stmt_reri_event, pos++, ev->suppl_info, -1);
	db_bind(&reri_event_tab, priv->stmt_reri_event, pos++, ev->timestamp_val, -1);

	rc = db_eval_stmt(priv->stmt_reri_event, "reri_event");
	if (!rc)
		log(TERM, LOG_INFO, "register inserted at db\n");

	return rc;
}
#endif

/*
 * Generic code
 */

int ras_mc_event_opendb(unsigned int cpu, struct ras_events *ras)
{
	int rc;
	struct ras_record_priv *priv;

	printf("Calling %s()\n", __func__);

	rc = db_open(NULL, cpu, ras, sizeof(*priv));
	if (rc)
		return -1;

	priv = ras->db_priv;

#ifdef HAVE_AER
	rc = db_create_table(ras->db, &aer_event_tab);
	if (!rc) {
		rc = db_prepare_insert_stmt(ras->db, &priv->stmt_aer_event,
					 &aer_event_tab);
		if (rc)
			return -1;
	}
#endif

#ifdef HAVE_EXTLOG
	rc = db_create_table(ras->db, &extlog_event_tab);
	if (!rc) {
		rc = db_prepare_insert_stmt(ras->db, &priv->stmt_extlog_record,
					 &extlog_event_tab);
		if (rc)
			return -1;
	}
#endif

#ifdef HAVE_MCE
	rc = db_create_table(ras->db, &mce_record_tab);
	if (!rc) {
		rc = db_prepare_insert_stmt(ras->db, &priv->stmt_mce_record,
					 &mce_record_tab);
		if (rc)
			return -1;
	}
#endif

#ifdef HAVE_NON_STANDARD
	rc = db_create_table(ras->db, &non_standard_event_tab);
	if (!rc) {
		rc = db_prepare_insert_stmt(ras->db, &priv->stmt_non_standard_record,
					 &non_standard_event_tab);
		if (rc)
			return -1;
	}
#endif

#ifdef HAVE_ARM
	rc = db_create_table(ras->db, &arm_event_tab);
	if (!rc) {
		rc = db_prepare_insert_stmt(ras->db, &priv->stmt_arm_record,
					 &arm_event_tab);
		if (rc)
			return -1;
	}
#endif
#ifdef HAVE_DEVLINK
	rc = db_create_table(ras->db, &devlink_event_tab);
	if (!rc) {
		rc = db_prepare_insert_stmt(ras->db, &priv->stmt_devlink_event,
					 &devlink_event_tab);
		if (rc)
			return -1;
	}
#endif

#ifdef HAVE_DISKERROR
	rc = db_create_table(ras->db, &diskerror_event_tab);
	if (!rc) {
		rc = db_prepare_insert_stmt(ras->db, &priv->stmt_diskerror_event,
					 &diskerror_event_tab);
		if (rc)
			return -1;
	}
#endif

#ifdef HAVE_MEMORY_FAILURE
	rc = db_create_table(ras->db, &mf_event_tab);
	if (!rc) {
		rc = db_prepare_insert_stmt(ras->db, &priv->stmt_mf_event,
					 &mf_event_tab);
		if (rc)
			return -1;
	}
#endif

#ifdef HAVE_CXL
	rc = db_create_table(ras->db, &cxl_poison_event_tab);
	if (!rc) {
		rc = db_prepare_insert_stmt(ras->db, &priv->stmt_cxl_poison_event,
					 &cxl_poison_event_tab);
		if (rc)
			return -1;
	}

	rc = db_create_table(ras->db, &cxl_aer_ue_event_tab);
	if (!rc) {
		rc = db_prepare_insert_stmt(ras->db, &priv->stmt_cxl_aer_ue_event,
					 &cxl_aer_ue_event_tab);
		if (rc)
			return -1;
	}

	rc = db_create_table(ras->db, &cxl_aer_ce_event_tab);
	if (!rc) {
		rc = db_prepare_insert_stmt(ras->db, &priv->stmt_cxl_aer_ce_event,
					 &cxl_aer_ce_event_tab);
		if (rc)
			return -1;
	}

	rc = db_create_table(ras->db, &cxl_overflow_event_tab);
	if (!rc) {
		rc = db_prepare_insert_stmt(ras->db, &priv->stmt_cxl_overflow_event,
					 &cxl_overflow_event_tab);
		if (rc)
			return -1;
	}

	rc = db_create_table(ras->db, &cxl_generic_event_tab);
	if (!rc) {
		rc = db_prepare_insert_stmt(ras->db, &priv->stmt_cxl_generic_event,
					 &cxl_generic_event_tab);
		if (rc)
			return -1;
	}

	rc = db_create_table(ras->db, &cxl_general_media_event_tab);
	if (!rc) {
		rc = db_prepare_insert_stmt(ras->db, &priv->stmt_cxl_general_media_event,
					 &cxl_general_media_event_tab);
		if (rc)
			return -1;
	}

	rc = db_create_table(ras->db, &cxl_dram_event_tab);
	if (!rc) {
		rc = db_prepare_insert_stmt(ras->db, &priv->stmt_cxl_dram_event,
					 &cxl_dram_event_tab);
		if (rc)
			return -1;
	}

	rc = db_create_table(ras->db, &cxl_memory_module_event_tab);
	if (!rc) {
		rc = db_prepare_insert_stmt(ras->db, &priv->stmt_cxl_memory_module_event,
					 &cxl_memory_module_event_tab);
		if (rc)
			return -1;
	}
#endif

#ifdef HAVE_SIGNAL
	rc = db_create_table(ras->db, &signal_event_tab);
	if (!rc) {
		rc = db_prepare_insert_stmt(ras->db, &priv->stmt_signal_event,
					 &signal_event_tab);
		if (rc)
			return -1;
	}
#endif

#ifdef HAVE_RERI
	rc = db_create_table(ras->db, &reri_event_tab);
	if (!rc) {
		rc = db_prepare_insert_stmt(ras->db, &priv->stmt_reri_event,
					 &reri_event_tab);
		if (rc)
			return -1;
	}
#endif

	return 0;
}

int ras_mc_event_closedb(unsigned int cpu, struct ras_events *ras)
{
	struct ras_record_priv *priv = ras->db_priv;
	struct ras_db *db;
	int rc = 0;

	printf("Calling %s()\n", __func__);

	if (ras->db_ref_count <= 0)
		return -1;
	if (ras->db_ref_count > 1)
		return db_close(cpu, ras);

	if (!priv)
		return -1;

	db = ras->db;
	if (!db)
		return -1;

	if (db_cpu_finalize(cpu, priv->stmt_mc_event, "mc_event"))
		rc = -1;

	if (db_cpu_finalize(cpu, priv->stmt_aer_event, "aer_event"))
		rc = -1;

	if (db_cpu_finalize(cpu, priv->stmt_extlog_record, "extlog_record"))
		rc = -1;

	if (db_cpu_finalize(cpu, priv->stmt_mce_record, "mce_record"))
		rc = -1;

	if (db_cpu_finalize(cpu, priv->stmt_non_standard_record, "non_standard_record"))
		rc = -1;

	if (db_cpu_finalize(cpu, priv->stmt_arm_record, "arm_record"))
		rc = -1;

	if (db_cpu_finalize(cpu, priv->stmt_devlink_event, "devlink_event"))
		rc = -1;

	if (db_cpu_finalize(cpu, priv->stmt_diskerror_event, "diskerror_event"))
		rc = -1;

	if (db_cpu_finalize(cpu, priv->stmt_mf_event, "mf_event"))
		rc = -1;

	if (db_cpu_finalize(cpu, priv->stmt_cxl_poison_event, "cxl_poison_event"))
		rc = -1;

	if (db_cpu_finalize(cpu, priv->stmt_cxl_aer_ue_event, "cxl_aer_ue_event"))
		rc = -1;

	if (db_cpu_finalize(cpu, priv->stmt_cxl_aer_ce_event, "cxl_aer_ce_event"))
		rc = -1;

	if (db_cpu_finalize(cpu, priv->stmt_cxl_overflow_event, "cxl_overflow_event"))
		rc = -1;

	if (db_cpu_finalize(cpu, priv->stmt_cxl_generic_event, "cxl_generic_event"))
		rc = -1;

	if (db_cpu_finalize(cpu, priv->stmt_cxl_general_media_event, "cxl_general_media_event"))
		rc = -1;

	if (db_cpu_finalize(cpu, priv->stmt_cxl_dram_event, "cxl_dram_event"))
		rc = -1;

	if (db_cpu_finalize(cpu, priv->stmt_cxl_memory_module_event, "cxl_memory_module_event"))
		rc = -1;

	if (db_cpu_finalize(cpu, priv->stmt_signal_event, "signal_event"))
		rc = -1;

	if (db_cpu_finalize(cpu, priv->stmt_reri_event, "reri_event"))
		rc = -1;

	if (db_close(cpu, ras))
		rc = -1;

	return rc;
}

#ifdef HAVE_UNITTEST
struct db_table_descriptor_list ras_record_table_descriptors(void)
{
	static const struct db_table_descriptor * const tables[] = {
		&mc_event_tab,
#ifdef HAVE_AER
		&aer_event_tab,
#endif
#ifdef HAVE_NON_STANDARD
		&non_standard_event_tab,
#endif
#ifdef HAVE_ARM
		&arm_event_tab,
#endif
#ifdef HAVE_EXTLOG
		&extlog_event_tab,
#endif
#ifdef HAVE_MCE
		&mce_record_tab,
#endif
#ifdef HAVE_DEVLINK
		&devlink_event_tab,
#endif
#ifdef HAVE_DISKERROR
		&diskerror_event_tab,
#endif
#ifdef HAVE_MEMORY_FAILURE
		&mf_event_tab,
#endif
#ifdef HAVE_CXL
		&cxl_poison_event_tab,
		&cxl_aer_ue_event_tab,
		&cxl_aer_ce_event_tab,
		&cxl_overflow_event_tab,
		&cxl_generic_event_tab,
		&cxl_general_media_event_tab,
		&cxl_dram_event_tab,
		&cxl_memory_module_event_tab,
#endif
#ifdef HAVE_SIGNAL
		&signal_event_tab,
#endif
#ifdef HAVE_RERI
		&reri_event_tab,
#endif
	};

	return (struct db_table_descriptor_list) {
		.tables = tables,
		.num_tables = ARRAY_SIZE(tables),
	};
}
#endif

#endif
