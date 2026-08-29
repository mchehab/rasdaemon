/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2026 Mauro Carvalho Chehab <mchehab+huawei@kernel.org>
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <sys/queue.h>
#include <unistd.h>

#include "actions/db-store.h"
#include "core/ras-events.h"
#include "core/ras-logger.h"
#include "db/ras-db-backend.h"
#include "db/ras-db.h"

/**
 * var selected_backend - name selected for the next implicit db_open()
 */
static const char *selected_backend = NULL;
/**
 * var ras_db_ops - operations for the currently open process-wide backend
 */
static const struct ras_db_backend_ops *ras_db_ops = NULL;

/**
 * struct ras_db_backend_runtime - registry wrapper for a database backend
 * @entry: static backend descriptor
 * @node: link in ras_db_backends
 */
struct ras_db_backend_runtime {
	const struct ras_db_backend_entry *entry;

	LIST_ENTRY(ras_db_backend_runtime) node;
};

LIST_HEAD(ras_db_backend_list, ras_db_backend_runtime);

/**
 * var ras_db_backends - registered database backends sorted by name
 */
static struct ras_db_backend_list ras_db_backends =
	LIST_HEAD_INITIALIZER(ras_db_backends);

/**
 * var rasdaemon_hostname - hostname attached to remote database records
 */
const char *rasdaemon_hostname = "";
/**
 * var rasdaemon_hostname_buf - storage for the system hostname
 */
static char rasdaemon_hostname_buf[256];

/**
 * var add_hostname - whether the active backend needs a hostname column
 */
static bool add_hostname = false;

/**
 * db_backend_register - register a static database backend descriptor
 * @entry: complete descriptor which remains valid until unregistered
 *
 * Registration is performed by module initialization and is not thread-safe.
 * Backends are sorted by name and duplicate names are rejected.
 *
 * Return:
 * * 0 - the backend was registered
 * * -EINVAL - @entry or its required operations are incomplete
 * * -EEXIST - its name is already registered
 * * -ENOMEM - registry-wrapper allocation failed
 */
int db_backend_register(struct ras_db_backend_entry *entry)
{
	const struct ras_db_backend_ops *ops;
	const char *name;
	struct ras_db_backend_runtime *new, *cur, *prev = NULL;

	if (!entry) {
		log(TERM, LOG_ERR, "Backend entry is missing!\n");
		return -EINVAL;
	}
	ops = entry->ops;
	name = entry->name;

	if (!name || !ops || !ops->get_sql_type || !ops->bind_type ||
	    !ops->eval_stmt || !ops->create_table || !ops->alter_table ||
	    !ops->prepare_stmt || !ops->finalize || !ops->open ||
	    !ops->close || !ops->db_exec_sql) {
		log(TERM, LOG_ERR, "Incomplete ops for backend %s\n", name);
		return -EINVAL;
	}

	LIST_FOREACH(cur, &ras_db_backends, node) {
		if (!strcmp(name, cur->entry->name))
			return -EEXIST;
	}

	new = malloc(sizeof(*new));
	if (!new) {
		log(ALL, LOG_ERR, "no memory to register module %s\n",
		    entry->name);
		return -ENOMEM;
	}
	new->entry = entry;

	/* Keep it alphabetically sorted */
	LIST_FOREACH(cur, &ras_db_backends, node) {
		if (strcmp(entry->name, cur->entry->name) < 0)
			break;

		prev = cur;
	}

	if (cur)
		LIST_INSERT_BEFORE(cur, new, node);
	else if (prev)
		LIST_INSERT_AFTER(prev, new, node);
	else
		LIST_INSERT_HEAD(&ras_db_backends, new, node);

	return 0;
}

/**
 * db_backend_unregister - remove a database backend descriptor
 * @entry: descriptor previously registered
 *
 * Return:
 * * 0 - the backend was unregistered
 * * -EINVAL - @entry is NULL
 * * -EBUSY - @entry is the active backend
 * * -ENOENT - @entry is not registered
 */
int db_backend_unregister(struct ras_db_backend_entry *entry)
{
	struct ras_db_backend_runtime *registered;

	if (!entry)
		return -EINVAL;

	if (ras_db_ops == entry->ops)
		return -EBUSY;

	LIST_FOREACH(registered, &ras_db_backends, node) {
		if (registered->entry != entry)
			continue;

		LIST_REMOVE(registered, node);
		free(registered);

		if (selected_backend &&
		    !strcmp(selected_backend, entry->name))
			selected_backend = NULL;

		return 0;
	}

	return -ENOENT;
}

/**
 * db_backend_is_registered - query a database backend name
 * @name: backend name
 *
 * Return:
 * true when registered; false for NULL or an unknown name.
 */
bool db_backend_is_registered(const char *name)
{
	const struct ras_db_backend_runtime *backend;

	if (!name)
		return false;

	LIST_FOREACH(backend, &ras_db_backends, node) {
		if (!strcmp(name, backend->entry->name))
			return true;
	}

	return false;
}

/**
 * db_list_available_backends - format registered backend names
 *
 * The returned static buffer is overwritten by the next call and is not
 * thread-safe.
 *
 * Return:
 * a comma-separated list of registered backend names, possibly empty.
 */
const char *db_list_available_backends(void)
{
	const struct ras_db_backend_runtime *backend;
	static char buf[256];
	int len = 0;

	buf[0] = '\0';

	LIST_FOREACH(backend, &ras_db_backends, node) {
		if (len > 0)
			strncat(buf, ", ", sizeof(buf) - len - 1);
		strncat(buf, backend->entry->name, sizeof(buf) - len - 1);
		len = (int)strlen(buf);
	}

	return buf;
}

/**
 * db_backend_enable - select the backend used by implicit db_open()
 * @name: explicit registered name, or NULL to read RASDAEMON_DB_BACKEND
 *
 * Selection does not open a connection and must precede db_open().
 *
 * Return:
 * * 0 - the backend was selected
 * * -1 - the requested backend is unavailable
 */
int db_backend_enable(const char *name)
{
	const struct ras_db_backend_runtime *registered = NULL;
	const char *backend;

	if (name)
		backend = name;
	else
		backend = env_or("RASDAEMON_DB_BACKEND", "sqlite3");

	LIST_FOREACH(registered, &ras_db_backends, node) {
		if (strcmp(registered->entry->name, backend) == 0)
			break;
	}

	if (!registered) {
		log(TERM, LOG_ERR,
		    "Backend '%s' not found. Available: %s\n",
		    backend, db_list_available_backends());
		return -1;
	}

	log(TERM, LOG_INFO, "Enabling DB backend: %s\n", backend);
	selected_backend = backend;
	return 0;
}

/**
 * db_get_rasdaemon_hostname - initialize the remote-record hostname
 *
 * RASDAEMON_HOSTNAME takes precedence over gethostname(). The resulting
 * process-lifetime string is stored in rasdaemon_hostname.
 */
static void db_get_rasdaemon_hostname(void)
{
	const char *env_hostname;

	/* Hostname already set */
	if (*rasdaemon_hostname)
		return;

	env_hostname = getenv("RASDAEMON_HOSTNAME");
	if (env_hostname != NULL && env_hostname[0] != '\0') {
		rasdaemon_hostname = env_hostname;
		return;
	}

	if (gethostname(rasdaemon_hostname_buf,
			sizeof(rasdaemon_hostname_buf)) != 0) {
		log(TERM, LOG_ERR, "Failed to get hostname\n");
		return;
	}

	rasdaemon_hostname_buf[sizeof(rasdaemon_hostname_buf) - 1] = '\0';
	rasdaemon_hostname = rasdaemon_hostname_buf;
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

/**
 * db_open - open or reference the process-wide database session
 * @backend: explicit test backend and parameters, or NULL for selected backend
 * @cpu: logical CPU passed to backend lifecycle callbacks
 * @ras: process context receiving connection and private state
 * @size_priv: bytes of zeroed backend-private storage to allocate
 *
 * Calls after the first successful open only increment @ras->db_ref_count.
 * The first open creates every registered table and prepared statement. A
 * partial table-open failure rolls back the connection and private state.
 * Database APIs are process-global and callers must serialize lifecycle calls.
 *
 * Return:
 * * 0 - the session opened or its reference count was incremented
 * * -EINVAL - @ras is NULL or no backend was explicitly/implicitly selected
 * * -ENOMEM - @size_priv bytes could not be allocated
 * * -1 - the selected backend is unavailable or its open callback failed
 * * otherwise - the table-opening error after the backend connection opened
 */
int db_open(struct db_backend *backend, unsigned int cpu,
	    struct ras_events *ras, size_t size_priv)
{
	const struct ras_db_backend_runtime *registered;
	const struct ras_db_backend_entry *entry;
	const char *backend_name;
	void *conn_parms;
	void *db_priv;
	int rc;

	if (!ras)
		return -EINVAL;

	if (backend)
		backend_name = backend->name;
	else
		backend_name = selected_backend;
	if (!backend_name)
		return -EINVAL;

	ras->db_ref_count++;
	if (ras->db_ref_count > 1) {
		log(TERM, LOG_INFO,
		    "Database was already opened.\n");
		return 0;
	}

	db_priv = size_priv ? calloc(1, size_priv) : NULL;
	if (size_priv && !db_priv) {
		log(TERM, LOG_ERR,
		    "Failed to allocate memory for backend\n");
		ras->db_ref_count--;
		return -ENOMEM;
	}

	LIST_FOREACH(registered, &ras_db_backends, node) {
		entry = registered->entry;
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
			rc = db_store_tables_open(ras, cpu);
			if (rc) {
				entry->ops->close(ras->db, cpu);
				ras_db_ops = NULL;
				ras->db = NULL;
				ras->db_priv = NULL;
				free(db_priv);
				ras->db_ref_count--;
				return rc;
			}
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

/**
 * db_close - drop a reference to the process-wide database session
 * @cpu: logical CPU passed to finalization callbacks
 * @ras: context used for db_open()
 *
 * Only the final matching close finalizes tables, closes the backend, and
 * releases @ras->db_priv. With database operations inactive this is a no-op.
 *
 * Return:
 * * 0 - no backend is active, a reference remains, or the final close succeeded
 * * -EINVAL - db_close() has no matching open reference
 * * -1 - table finalization failed and the backend close succeeded
 * * otherwise - the backend close error
 */
int db_close(unsigned int cpu, struct ras_events *ras)
{
	int rc, table_rc;

	if (unlikely(!ras_db_ops))
		return 0;
	if (ras->db_ref_count <= 0)
		return -EINVAL;

	ras->db_ref_count--;

	if (ras->db_ref_count > 0)
		return 0;

	table_rc = db_store_tables_close(cpu);
	rc = ras_db_ops->close(ras->db, cpu);
	ras_db_ops = NULL;
	free(ras->db_priv);
	ras->db = NULL;
	ras->db_priv = NULL;

	return rc ? rc : table_rc;
}

/**
 * db_get_sql_type - map a portable field type to backend SQL
 * @type: portable column type
 * @is_pk: whether the column is a primary key
 *
 * Return:
 * a backend-owned SQL type string, or an empty string when no backend is
 * active. The caller must not free the result.
 */
const char *db_get_sql_type(enum db_field_type type, bool is_pk)
{
	if (unlikely(!ras_db_ops))
		return "";

	return ras_db_ops->get_sql_type(type, is_pk);
}

/**
 * db_bind_type - bind a value with an explicit portable field type
 * @stmt: active prepared statement
 * @type: portable field type
 * @pos: one-based placeholder position
 * @value: scalar value or pointer encoded as uint64_t
 * @len: byte length for variable-sized data
 *
 * Return:
 * * 0 - no backend is active or the value was bound
 * * -EINVAL - @stmt is NULL while a backend is active
 * * otherwise - the backend binding error
 */
static int db_bind_type(struct ras_stmt *stmt, const enum db_field_type type,
			int pos, uint64_t value, int len)
{
	if (unlikely(!ras_db_ops))
		return 0;

	if (unlikely(!stmt))
		return -EINVAL;

	return ras_db_ops->bind_type(stmt, type, pos, value, len);
}

/**
 * db_bind - bind one non-serial descriptor field by placeholder position
 * @db_tab: table descriptor
 * @stmt: active prepared statement
 * @pos: one-based placeholder excluding serial fields
 * @value: scalar value or pointer encoded as uint64_t
 * @len: byte length for text/blob data
 *
 * Return:
 * * 0 - no backend is active or the value was bound
 * * -1 - @pos does not identify a non-serial field
 * * -EINVAL - @stmt is NULL while a backend is active
 * * otherwise - the backend binding error
 */
int db_bind(const struct db_table_descriptor *db_tab,
	    struct ras_stmt *stmt, int pos, uint64_t value, int len)
{
	const struct db_fields *fields = db_tab->fields;
	int i, field_pos = 0;

	if (unlikely(!ras_db_ops))
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

/**
 * db_eval_stmt - execute a prepared statement
 * @stmt: active prepared statement
 * @tab_name: table name used in diagnostics
 *
 * Return:
 * * 0 - no backend is active or execution succeeded
 * * -EINVAL - @stmt is NULL while a backend is active
 * * otherwise - the backend execution error
 */
int db_eval_stmt(struct ras_stmt *stmt, const char *tab_name)
{
	if (unlikely(!ras_db_ops))
		return 0;

	if (unlikely(!stmt))
		return -EINVAL;

	return ras_db_ops->eval_stmt(stmt, tab_name);
}

/**
 * db_create_table - create a descriptor-defined table
 * @db: active backend connection
 * @db_tab: persistent table descriptor
 *
 * Return:
 * 0 without an active backend or on success; otherwise the backend
 * table-creation error.
 */
int db_create_table(struct ras_db *db, const struct db_table_descriptor *db_tab)
{
	if (unlikely(!ras_db_ops))
		return 0;

	return ras_db_ops->create_table(db, db_tab);
}

/**
 * db_alter_table - update an existing table to a descriptor
 * @db: active backend connection
 * @stmt: backend scratch/output statement pointer
 * @db_tab: persistent table descriptor
 *
 * Return:
 * 0 without an active backend or on success; otherwise the backend
 * table-alteration error.
 */
int db_alter_table(struct ras_db *db, struct ras_stmt **stmt,
		   const struct db_table_descriptor *db_tab)
{
	if (unlikely(!ras_db_ops))
		return 0;

	return ras_db_ops->alter_table(db, stmt, db_tab);
}

/**
 * db_prepare_insert_stmt - prepare a table's insert statement
 * @db: active backend connection
 * @stmt: destination owned by the table registration until finalized
 * @db_tab: persistent table descriptor
 *
 * Return:
 * 0 without an active backend or on success; otherwise the backend
 * statement-preparation error.
 */
int db_prepare_insert_stmt(struct ras_db *db, struct ras_stmt **stmt,
			   const struct db_table_descriptor *db_tab)
{
	if (unlikely(!ras_db_ops))
		return 0;
	return ras_db_ops->prepare_stmt(db, stmt, db_tab);
}

/**
 * db_exec_sql - execute an unprepared SQL command
 * @db: active backend connection
 * @sql: command string valid for the selected backend
 *
 * Return:
 * 0 without an active backend or on success; otherwise the backend execution
 * error.
 */
int db_exec_sql(struct ras_db *db, const char *sql)
{
	if (unlikely(!ras_db_ops))
		return 0;

	return ras_db_ops->db_exec_sql(db, sql);
}

/**
 * db_finalize - finalize a non-CPU-specific prepared statement
 * @stmt: statement to finalize, or NULL
 *
 * Return:
 * 0 without an active backend, when @stmt is NULL, or on success; otherwise
 * the backend finalization error.
 */
int db_finalize(struct ras_stmt *stmt)
{
	if (unlikely(!ras_db_ops))
		return 0;

	if (unlikely(!stmt))
		return 0;

	return ras_db_ops->finalize(-1, stmt, NULL);
}

/**
 * db_cpu_finalize - finalize a named CPU-associated statement
 * @cpu: logical CPU passed to the backend
 * @stmt: statement to finalize
 * @name: diagnostic/prepared-statement name
 *
 * Return:
 * 0 without an active backend or on success; otherwise the backend
 * finalization error.
 */
int db_cpu_finalize(unsigned int cpu, struct ras_stmt *stmt, const char *name)
{
		if (unlikely(!ras_db_ops))
			return 0;

		return ras_db_ops->finalize(cpu, stmt, name);
}
