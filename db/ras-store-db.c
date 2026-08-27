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

#include "core/ras-events.h"
#include "core/ras-logger.h"
#include "db/ras-store-db.h"

#ifdef HAVE_DB

int ras_db_features_open(struct ras_events *ras);
int ras_db_features_close(unsigned int cpu, struct ras_events *ras);

/*
 * Table and functions to handle ras:mc_event
 */

static const struct db_fields mc_event_fields[] = {
	{ .name = "id",			.type = DB_TYPE_SERIAL, .is_pk = true },
	{ .name = "timestamp",		.type = DB_TYPE_TIMESTAMP, .create_index = true },
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

const struct db_table_descriptor mc_event_tab = {
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

	rc = db_create_table(ras->db, &mc_event_tab);
	if (rc)
		return -1;

	rc = db_prepare_insert_stmt(ras->db, &priv->stmt_mc_event,
				    &mc_event_tab);
	if (rc)
		return -1;

	rc = ras_db_features_open(ras);
	if (rc)
		return -1;

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
	if (ras_db_features_close(cpu, ras))
		rc = -1;

	if (db_close(cpu, ras))
		rc = -1;

	return rc;
}

#endif
