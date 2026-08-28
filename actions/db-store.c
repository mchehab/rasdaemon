// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2026 Mauro Carvalho Chehab <mchehab+huawei@kernel.org>
 */

#include "config.h"

#include <errno.h>
#include <stdlib.h>
#include <sys/queue.h>

#include "actions/db-store.h"
#include "core/modules.h"
#include "core/ras-events.h"
#include "core/ras-logger.h"
#include "db/ras-db.h"

struct ras_db_table_runtime {
	struct ras_module_ctx *ctx;
	struct db_desc_and_stmt *entry;

	LIST_ENTRY(ras_db_table_runtime) node;
};

LIST_HEAD(ras_db_table_list, ras_db_table_runtime);

static struct ras_db_table_list ras_db_tables =
	LIST_HEAD_INITIALIZER(ras_db_tables);

int db_store_tables_close(unsigned int cpu)
{
	struct ras_db_table_runtime *table;
	int rc = 0;

	LIST_FOREACH(table, &ras_db_tables, node) {
		if (table->entry->stmt &&
		    db_cpu_finalize(cpu, table->entry->stmt,
				    table->entry->desc->name))
			rc = -1;
		table->entry->stmt = NULL;
	}

	return rc;
}

int db_store_tables_open(struct ras_events *ras, unsigned int cpu)
{
	struct ras_db_table_runtime *table;
	int rc;

	LIST_FOREACH(table, &ras_db_tables, node) {
		rc = db_create_table(ras->db, table->entry->desc);
		if (!rc)
			rc = db_prepare_insert_stmt(ras->db, &table->entry->stmt,
						    table->entry->desc);
		if (rc)
			log(TERM, LOG_ERR, "Failed to open table %s: %d\n",
			    table->entry->desc->name, rc);
		if (rc) {
			db_store_tables_close(cpu);
			return rc;
		}
	}

	return 0;
}

int ras_db_table_register(struct ras_module_ctx *ctx,
			  struct db_desc_and_stmt *entry)
{
	struct ras_db_table_runtime *new, *registered, *prev = NULL;

	if (!ctx || !entry || !entry->desc)
		return -EINVAL;

	LIST_FOREACH(registered, &ras_db_tables, node) {
		if ((registered->ctx == ctx && registered->entry == entry) ||
		    registered->entry->desc == entry->desc)
			return -EEXIST;

		prev = registered;
	}

	new = calloc(1, sizeof(*new));
	if (!new)
		return -ENOMEM;

	new->ctx = ctx;
	new->entry = entry;
	if (prev)
		LIST_INSERT_AFTER(prev, new, node);
	else
		LIST_INSERT_HEAD(&ras_db_tables, new, node);

	return 0;
}

#ifdef HAVE_UNITTEST
int ras_db_table_test_foreach(ras_db_table_test_callback callback, void *data)
{
	const struct ras_db_table_runtime *table;
	int rc;

	if (!callback)
		return -EINVAL;

	LIST_FOREACH(table, &ras_db_tables, node) {
		rc = callback(table->entry->desc, data);
		if (rc)
			return rc;
	}

	return 0;
}
#endif

void ras_db_table_unregister(struct ras_module_ctx *ctx)
{
	struct ras_db_table_runtime *registered, *next;

	if (!ctx)
		return;

	registered = LIST_FIRST(&ras_db_tables);
	while (registered) {
		next = LIST_NEXT(registered, node);
		if (registered->ctx == ctx) {
			LIST_REMOVE(registered, node);
			free(registered);
		}
		registered = next;
	}
}

static int db_store_consume(struct ras_events *ras, int event, void *data)
{
	if (!ras->record_events)
		return 0;

	return ras_event_record(ras, event, data);
}

static const struct ras_event_consumer db_store_consumer = {
	.name = "db-store",
	.priority = PRI_DB_RECORD,
	.events = BIT_ULL(NR_EVENTS) - 1,
	.consume = db_store_consume,
};

REGISTER_RAS_EVENT_CONSUMER(db_store_consumer);
