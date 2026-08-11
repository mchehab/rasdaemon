/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2026 Mauro Carvalho Chehab <mchehab+huawei@kernel.org>
 */

#include <stdlib.h>
#include <string.h>

#include "config.h"

#include "core/ras-events.h"
#include "core/ras-logger.h"

#include "db/ras-db.h"
#include "db/ras-db-backend.h"

struct ras_db_backend_entry ras_db_backends = { 0 };

int db_backend_register(struct ras_db_backend_entry *entry)
{
	const struct ras_db_backend_ops *ops = entry->ops;
	const char *name = entry->name;
	struct ras_db_backend_entry **head = &ras_db_backends.next;
	struct ras_db_backend_entry *new, *cur, *prev = NULL;

	if (!ops || !ops->get_sql_type || !ops->bind_type || !ops->bind ||
	    !ops->eval_stmt || !ops->create_table || !ops->alter_table ||
	    !ops->prepare_stmt || !ops->finalize || !ops->open ||
	    !ops->close) {
		log(TERM, LOG_ERR, "Incomplete ops for backend %s\n", name);
		return -EINVAL;
	}

	if (!entry) {
		log(TERM, LOG_ERR, "Backend entry is missing!\n");
		return -EINVAL;
	}

	new = malloc(sizeof(*new));
	if (!new) {
		log(ALL, LOG_ERR, "no memory to register module %s\n",
		    entry->name);
		return -ENOMEM;
	}
	memcpy(new, entry, sizeof(*entry));

	/* Keep it alphabetically sorted */
	for (cur = ras_db_backends.next; cur; cur = cur->next) {
		if (strcmp(entry->name, cur->name) < 0)
			break;

		prev = cur;
	}

	if (!prev) {
		new->next =*head;
		*head = new;
	} else {
		new->next = prev->next;
		prev->next = new;
	}

	return 0;
}

/*
 * Callback wrappers.
 *
 * NOTE: They don't need to check if ops->callback is not
 *	 NULL, as the register code above already warrants it.
 */

/*
 * While multiple backends can be compiled, only one can be active,
 * as we're storing database pointers inside ras->db and we have only
 * one db_priv at ras_events. To keep it simple, use a static var to
 * store the active backend,
 */

const struct ras_db_backend_ops *ops = NULL;

int db_open(unsigned int cpu, struct ras_events *ras, size_t size_priv)
{
	struct ras_db_backend_entry *entry = &ras_db_backends;
	struct ras_db *db;
	int rc;

	ras->db_ref_count++;
	if (ras->db_ref_count > 1)
		return 0;

	db = calloc(1, size_priv);
	if (!db)
		return -ENOMEM;

	entry = entry->next;
	while (entry) {
		rc = entry->ops->open(ras->db, cpu);
		if (rc >= 0) {
		ras->db = db;
		ops = entry->ops;
		return 0;
		}
		entry = entry->next;
	}
	free(db);

	return -1;
}

int db_close(unsigned int cpu, struct ras_events *ras)
{
	int rc;

	if (!ops)
		return 0;

	ras->db_ref_count--;

	if (ras->db_ref_count > 0)
		return 0;

	rc = ops->close(ras->db, cpu);
	ops = NULL;
	free(ras->db);
	ras->db = NULL;

	return rc;
}

const char *db_get_sql_type(enum db_field_type type, bool is_pk)
{
	if (!ops)
		return "";

	return ops->get_sql_type(type, is_pk);
}

void db_bind_type(struct ras_stmt *stmt, const enum db_field_type type,
		  int pos, uint64_t value, int len)
{
	if (ops)
		ops->bind_type(stmt, type, pos, value, len);
}

void db_bind(struct ras_stmt *stmt, const struct db_fields *fields,
	     int pos, uint64_t value, int len)
{
	if (ops)
		ops->bind(stmt, fields, pos, value, len);
}

int db_eval_stmt(struct ras_stmt *stmt, const char *tab_name)
{
	if (!ops)
		return 0;

	return ops->eval_stmt(stmt, tab_name);
}

int db_create_table(struct ras_db *db, const struct db_table_descriptor *db_tab)
{
	if (!ops)
		return 0;

	return ops->create_table(db, db_tab);
}

int db_alter_table(struct ras_db *db, struct ras_stmt **stmt,
		   const struct db_table_descriptor *db_tab)
{
	if (!ops)
		return 0;
	return ops->alter_table(db, stmt, db_tab);
}

int db_prepare_stmt(struct ras_db *db, struct ras_stmt **stmt,
		    const struct db_table_descriptor *db_tab)
{
	if (!ops)
		return 0;
	return ops->prepare_stmt(db, stmt, db_tab);
}

int db_create_table_prep_stmt(struct ras_events *ras, struct ras_stmt **stmt,
			      const struct db_table_descriptor *db_tab)
{
	int rc;

	if (!ops)
		return 0;

	rc = ops->create_table(ras->db, db_tab);
	if (rc)
		return rc;

	return ops->prepare_stmt(ras->db, stmt, db_tab);
}

int db_finalize(struct ras_stmt *stmt)
{
	if (!ops)
		return 0;

	if (!stmt)
		return 0;

	return ops->finalize(-1, stmt, NULL);
}

int db_cpu_finalize(unsigned int cpu, struct ras_stmt *stmt, const char *name)
{
		if (!ops)
			return 0;

		return ops->finalize(cpu, stmt, name);
}

