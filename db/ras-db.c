/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2026 Mauro Carvalho Chehab <mchehab+huawei@kernel.org>
 */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "config.h"

#include "core/ras-events.h"
#include "core/ras-logger.h"

#include "db/ras-db.h"
#include "db/ras-db-backend.h"

const char *selected_backend = NULL;
struct ras_db_backend_entry ras_db_backends = { 0 };
const struct ras_db_backend_ops *ras_db_ops = NULL;

const char *rasdaemon_hostname = "";

static bool add_hostname = false;

int db_backend_register(struct ras_db_backend_entry *entry)
{
	const struct ras_db_backend_ops *ops;
	const char *name;
	struct ras_db_backend_entry **head = &ras_db_backends.next;
	struct ras_db_backend_entry *new, *cur, *prev = NULL;

	if (!entry) {
		log(TERM, LOG_ERR, "Backend entry is missing!\n");
		return -EINVAL;
	}
	ops = entry->ops;
	name = entry->name;

	if (!ops || !ops->get_sql_type || !ops->bind_type ||
	    !ops->eval_stmt || !ops->create_table || !ops->alter_table ||
	    !ops->prepare_stmt || !ops->finalize || !ops->open ||
	    !ops->close || !ops->db_exec_sql) {
		log(TERM, LOG_ERR, "Incomplete ops for backend %s\n", name);
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
		new->next = *head;
		*head = new;
	} else {
		new->next = prev->next;
		prev->next = new;
	}

	return 0;
}

const char *db_list_available_backends(void)
{
	const struct ras_db_backend_entry *entry;
	static char buf[256];
	int len = 0;

	buf[0] = '\0';

	for (entry = ras_db_backends.next; entry; entry = entry->next) {
		if (len > 0)
			strncat(buf, ", ", sizeof(buf) - len - 1);
		strncat(buf, entry->name, sizeof(buf) - len - 1);
		len = (int)strlen(buf);
	}

	return buf;
}

int db_backend_enable(const char *name)
{
	const struct ras_db_backend_entry *entry = NULL;
	const char *backend;

	if (name)
		backend = name;
	else
		backend = env_or("RASDAEMON_DB_BACKEND", "sqlite3");

	for (entry = ras_db_backends.next; entry; entry = entry->next) {
		if (strcmp(entry->name, backend) == 0)
			break;
	}

	if (!entry) {
		log(TERM, LOG_ERR,
		    "Backend '%s' not found. Available: %s\n",
		    backend, db_list_available_backends());
		return -1;
	}

	log(TERM, LOG_INFO, "Enabling DB backend: %s\n", backend);
	selected_backend = backend;
	return 0;
}

static void db_get_rasdaemon_hostname(void)
{
	const char *env_hostname;
	char hostname[256];

	/* Hostname already set */
	if (*rasdaemon_hostname)
		return;

	env_hostname = getenv("RASDAEMON_HOSTNAME");
	if (env_hostname != NULL && env_hostname[0] != '\0') {
		rasdaemon_hostname = env_hostname;
		return;
	}

	if (gethostname(hostname, sizeof(hostname)) != 0) {
		log(TERM, LOG_ERR, "Failed to get hostname\n");
		return;
	}

	rasdaemon_hostname = strdup(hostname);
}

/*
 * Callback wrappers.
 *
 * NOTE: They don't need to check if ras_db_ops->callback is not
 *	 NULL, as the register code above already warrants it.
 */

/*
 * While multiple backends can be compiled, only one can be active,
 * as we're storing database pointers inside ras->db and we have only
 * one db_priv at ras_events. To keep it simple, use a static var to
 * store the active backend,
 */

int db_open(struct db_backend *backend, unsigned int cpu,
	    struct ras_events *ras, size_t size_priv)
{
	struct ras_db_backend_entry *entry = &ras_db_backends;
	const char *backend_name;
	void *conn_parms;
	void *db_priv;
	int rc;

	ras->db_ref_count++;
	if (ras->db_ref_count > 1) {
		log(TERM, LOG_INFO,
		    "Database was already opened.\n");
		return 0;
	}

	db_priv = calloc(1, size_priv);
	if (!db_priv) {
		log(TERM, LOG_ERR,
		    "Failed to allocate memory for backend\n");
		ras->db_ref_count--;
		return -ENOMEM;
	}

	if (backend && backend->name)
		backend_name = backend->name;
	else
		backend_name = selected_backend;

	for (entry = entry->next; entry; entry = entry->next) {
		if (strcmp(backend_name, entry->name)) {
			continue;
		}

		if (backend)
			conn_parms = backend->conn_parms;
		else if (entry->ops->get_conn_parms)
			conn_parms = entry->ops->get_conn_parms();
		else
			conn_parms = NULL;

		if (entry->allow_remote) {
			add_hostname = true;
			db_get_rasdaemon_hostname();
		} else {
			add_hostname = false;
		}

		rc = entry->ops->open(&ras->db, conn_parms, cpu);
		if (!rc) {
			ras->db_priv = db_priv;
			ras_db_ops = entry->ops;
			log(TERM, LOG_INFO,
			    "Database backend started: %s.\n", entry->name);

			return 0;
		}
	}
	log(TERM, LOG_INFO, "Database backend %s not found.\n",	backend_name);
	free(db_priv);

	ras->db_ref_count--;
	return -1;
}

int db_close(unsigned int cpu, struct ras_events *ras)
{
	int rc;

	if (!ras_db_ops)
		return 0;
	if (ras->db_ref_count <= 0)
		return -EINVAL;

	ras->db_ref_count--;

	if (ras->db_ref_count > 0)
		return 0;

	rc = ras_db_ops->close(ras->db, cpu);
	ras_db_ops = NULL;
	free(ras->db_priv);
	ras->db = NULL;

	return rc;
}

const char *db_get_sql_type(enum db_field_type type, bool is_pk)
{
	if (!ras_db_ops)
		return "";

	return ras_db_ops->get_sql_type(type, is_pk);
}

int db_bind_type(struct ras_stmt *stmt, const enum db_field_type type,
		  int pos, uint64_t value, int len)
{
	if (!ras_db_ops)
		return 0;

	return ras_db_ops->bind_type(stmt, type, pos, value, len);
}

int db_bind(const struct db_table_descriptor *db_tab,
	    struct ras_stmt *stmt, int pos, uint64_t value, int len)
{
	const struct db_fields *fields = db_tab->fields;
	int i, field_pos = 0;

	if (!ras_db_ops)
		return 0;

	if (pos < 1) {
		log(TERM, LOG_INFO, "table %s: invalid placeholder: %d\n",
		    db_tab->name, pos);
		return -1;
	}

	for (i = 0; i < db_tab->num_fields; i++) {
		if (fields[i].type == DB_TYPE_SERIAL) {
			continue;
		}

		if (field_pos == pos - 1)
			break;

		field_pos++;
	}
	if (field_pos != pos - 1) {
		log(TERM, LOG_INFO, "table %s: invalid placeholder: %d\n",
		    db_tab->name, pos);
		return -1;
	}

	return db_bind_type(stmt, db_tab->fields[i].type,
			    pos, value, len);
}

int db_eval_stmt(struct ras_stmt *stmt, const char *tab_name)
{
	if (!ras_db_ops)
		return 0;

	return ras_db_ops->eval_stmt(stmt, tab_name);
}

int db_create_table(struct ras_db *db, const struct db_table_descriptor *db_tab)
{
	if (!ras_db_ops)
		return 0;

	return ras_db_ops->create_table(db, db_tab);
}

int db_alter_table(struct ras_db *db, struct ras_stmt **stmt,
		   const struct db_table_descriptor *db_tab)
{
	if (!ras_db_ops)
		return 0;

	return ras_db_ops->alter_table(db, stmt, db_tab);
}

int db_prepare_insert_stmt(struct ras_db *db, struct ras_stmt **stmt,
			   const struct db_table_descriptor *db_tab)
{
	if (!ras_db_ops)
		return 0;
	return ras_db_ops->prepare_stmt(db, stmt, db_tab);
}

int db_create_table_prep_stmt(struct ras_events *ras, struct ras_stmt **stmt,
			      const struct db_table_descriptor *db_tab)
{
	int rc;

	if (!ras_db_ops)
		return 0;

	rc = ras_db_ops->create_table(ras->db, db_tab);
	if (rc)
		return rc;

	return ras_db_ops->prepare_stmt(ras->db, stmt, db_tab);
}

int db_exec_sql(struct ras_db *db, const char *sql)
{
	if (!ras_db_ops)
		return 0;

	return ras_db_ops->db_exec_sql(db, sql);
}

int db_finalize(struct ras_stmt *stmt)
{
	if (!ras_db_ops)
		return 0;

	if (!stmt)
		return 0;

	return ras_db_ops->finalize(-1, stmt, NULL);
}

int db_cpu_finalize(unsigned int cpu, struct ras_stmt *stmt, const char *name)
{
		if (!ras_db_ops)
			return 0;

		return ras_db_ops->finalize(cpu, stmt, name);
}
