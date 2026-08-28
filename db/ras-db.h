/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2026 Mauro Carvalho Chehab <mchehab+huawei@kernel.org>
 */

#ifndef RAS_DB_H
#define RAS_DB_H

#include "config.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/**
 * db_backend_enable - select backend to use
 * @name: name of the backend. NULL to allow selecting via env vars
 * Returns: if the env var exists, return its content; otherwise returns @def.
 */
/* #define DEBUG_SQL 1 */

/* Opaque types to make SQL data structs generic */
struct ras_db;
struct ras_stmt;
struct ras_events;
struct ras_module_ctx;

/**
 * enum db_field_type - Supported database column types
 * @DB_TYPE_SERIAL:	Auto-increment integer
 * @DB_TYPE_INT32:	32-bit signed integer
 * @DB_TYPE_INT64:	64-bit signed integer
 * @DB_TYPE_TIMESTAMP:	ISO timestamp (string or numeric value)
 * @DB_TYPE_TEXT:	Variable-length string
 * @DB_TYPE_BLOB:	Binary data
 */
enum db_field_type {
	DB_TYPE_SERIAL,
	DB_TYPE_INT32,
	DB_TYPE_INT64,
	DB_TYPE_TIMESTAMP,
	DB_TYPE_TEXT,
	DB_TYPE_BLOB,
};

/**
 * env_or - ancillary routine to get an environment with a default value
 * @name: name of the variable
 * @def: default value
 * Returns: if the env var exists, return its content; otherwise returns @def.
 */
static inline const char *env_or(const char *name, const char *def)
{
	const char *v = getenv(name);

	return (v && v[0]) ? v : def;
}

/**
 * env_or_bool - get a boolean from an environment variable
 * @name: name of the variable
 * @def: default value (0 or 1)
 * Returns: false if env var is false, 0 or no; true otherwise.
 */
static inline int env_or_bool(const char *name, int def)
{
	const char *v = getenv(name);

	if (!v || !v[0])
		return def;

	return (!strcmp(v, "0") ||
		!strcmp(v, "false") ||
		!strcmp(v, "no")) ? false : true;
}

/**
 * env_or_int - get an integer from an environment variable
 * @name: name of the variable
 * @def: default value
 * Returns: the parsed integer, or @def if parsing fails.
 */
static inline int env_or_int(const char *name, int def)
{
	const char *v = getenv(name);
	int val;

	if (!v || !v[0])
		return def;

	val = atoi(v);
	return (val) ? val : def;
}

/**
 * struct db_fields - Definition of a single column in a table descriptor
 * @name:	Column name identifier
 * @type:	Data type enumeration for binding logic
 * @is_pk:	True if this field is the primary key (affects SQL generation)
 * @create_index: True if an index should be created for this field
 */
struct db_fields {
	const char		*name;
	enum db_field_type	type;
	bool			is_pk;
	bool			create_index;
};

/**
 * struct db_table_descriptor - Metadata describing a database table schema
 * @name:	Name of the table in the database
 * @fields:	Array of column definitions
 * @num_fields:	Total count of columns in the array
 */
struct db_table_descriptor {
	const char			*name;
	const struct db_fields		*fields;
	size_t				num_fields;
};

/**
 * struct db_desc_and_stmt - A table descriptor and its prepared statement
 * @desc: Database table schema
 * @stmt: Storage for the prepared insert statement
 */
struct db_desc_and_stmt {
	const struct db_table_descriptor *desc;
	struct ras_stmt *stmt;
};

#ifdef HAVE_UNITTEST
struct db_table_descriptor_list {
	const struct db_table_descriptor * const *tables;
	size_t num_tables;
};

struct db_table_descriptor_list ras_record_table_descriptors(void);
struct db_table_descriptor_list ampere_table_descriptors(void);
struct db_table_descriptor_list hip08_table_descriptors(void);
struct db_table_descriptor_list hisilicon_table_descriptors(void);
struct db_table_descriptor_list jaguarmicro_table_descriptors(void);
struct db_table_descriptor_list nvidia_table_descriptors(void);
struct db_table_descriptor_list yitian_table_descriptors(void);
#endif

/**
 * struct db_backend - Specify what DB backend will be used
 * @backend:	Name of backend driver
 * @conn_parms:	Connection parameters (specific to each type of DB
 */
struct db_backend {
	const char *name;
	void *conn_parms;
};

#ifdef HAVE_DB

extern const char *rasdaemon_hostname;

/**
 * db_backend_enable - select backend to use
 * @name: name of the backend. NULL to allow selecting via env vars
 * Returns: if the env var exists, return its content; otherwise returns @def.
 */
int db_backend_enable(const char *name);
const char *db_list_available_backends(void);
/**
 * ras_db_table_register - Register one module-owned table pair
 * @ctx: Owning module context
 * @entry: Descriptor and statement pair that remains valid until unregistered
 */
int ras_db_table_register(struct ras_module_ctx *ctx,
			  struct db_desc_and_stmt *entry);

/**
 * ras_db_table_unregister - Remove every table pair owned by a module
 * @ctx: Owning module context
 *
 * The owner must finalize its statements before calling this function.
 */
void ras_db_table_unregister(struct ras_module_ctx *ctx);

/**
 * ops_bind - Bind fields from an event structure using fields definition
 * @db_tab:	Database table descriptor
 * @stmt:	Statement handle provided by the backend
 * @pos:	Starting position index placeholder (starts with 1)
 * @value:	Pointer to raw data buffer containing all field values
 * @len:	Length of the buffer (optional, depends on implementation)
 * Returns:
 * 0 on success or a negative errno value on failure.
 */
int db_bind(const struct db_table_descriptor *db_tab,
	    struct ras_stmt *stmt, int pos, uint64_t value, int len);

/**
 * db_get_sql_type - Return SQL column type string for a given field type
 * @type:	Field type descriptor from the enum db_field_type
 * @is_pk:	True if the column is declared as primary key in SQL
 *
 * Returns:
 * an opaque C-string describing the SQL type to use, e.g.
 * "INTEGER PRIMARY KEY", "TEXT", or "BLOB".  The caller must free
 * the returned string when it is no longer needed.
 */
const char *db_get_sql_type(enum db_field_type type, bool is_pk);

/**
 * db_eval_stmt - Execute a prepared SQL statement
 * @stmt:	Prepared statement handle to execute
 * @tab_name:	Name of the table whose data should be read/written
 *
 * Returns:
 * 0 on success or a negative errno value on failure.
 */
int db_eval_stmt(struct ras_stmt *stmt, const char *tab_name);

/**
 * db_create_table - Create a new database table from its descriptor
 * @db:		Database connection handle (opaque)
 * @db_tab:	Table descriptor containing the schema to create
 *
 * Returns:
 * 0 on success or a negative errno value on failure.
 */
int db_create_table(struct ras_db *db,
		    const struct db_table_descriptor *db_tab);

/**
 * db_alter_table - Modify an existing database table structure
 * @db:		Database connection handle (opaque)
 * @stmt:	Output pointer for a prepared statement describing the change
 * @db_tab:	Table descriptor containing the new schema to apply
 *
 * Returns:
 * 0 on success or a negative errno value on failure.
 */
int db_alter_table(struct ras_db *db,
		   struct ras_stmt **stmt,
		   const struct db_table_descriptor *db_tab);

/**
 * db_prepare_insert_stmt - Prepare a generic SQL statement for execution
 * @db:	Database connection handle (opaque)
 * @stmt:	Output pointer for the prepared statement handle
 * @db_tab:	Table descriptor providing context for the query
 *
 * Returns:
 * 0 on success or a negative errno value on failure.
 */
int db_prepare_insert_stmt(struct ras_db *db,
		    struct ras_stmt **stmt,
		    const struct db_table_descriptor *db_tab);

/**
 * db_create_table_prep_stmt - Prepare a CREATE TABLE statement for caching
 * @ras:	RAS events context (opaque)
 * @stmt:	Output pointer for the prepared statement handle
 * @db_tab:	Table descriptor containing the schema to create
 *
 * Returns:
 * 0 on success or a negative errno value on failure.
 */
int db_create_table_prep_stmt(struct ras_events *ras, struct ras_stmt **stmt,
			      const struct db_table_descriptor *db_tab);


/**
 * db_exec_sql - Execute a SQL statement
 * @stmt:	Prepared statement handle to finalize
 * @sql:	SQL command to execute
 *
 * Returns:
 * 0 on success or a negative errno value on failure.
 */
int db_exec_sql(struct ras_db *db, const char *sql);

/**
 * db_finalize - Finalize and release resources for a prepared statement
 * @stmt:	Prepared statement handle to finalize
 *
 * Returns:
 * 0 on success or a negative errno value on failure.
 */
int db_finalize(struct ras_stmt *stmt);

/**
 * db_cpu_finalize - CPU-local cleanup of a prepared statement resource
 * @cpu:	Logical CPU number for per-CPU bookkeeping (opaque)
 * @stmt:	Prepared statement handle to finalize
 * @name:	Name string for the resource being cleaned up
 *
 * Returns:
 * 0 on success or a negative errno value on failure..conn
 */
int db_cpu_finalize(unsigned int cpu, struct ras_stmt *stmt, const char *name);

/**
 * db_open - Open and initialize a database connection
 * @cpu:	Logical CPU number for per-CPU bookkeeping (opaque)
 * @ras:	RAS events context (opaque)
 * @size_priv:	Private memory size to allocate for the database handle
 *
 * Returns:
 * 0 on success or a negative errno value on failure.
 */
int db_open(struct db_backend *backend, unsigned int cpu,
	    struct ras_events *ras, size_t size_priv);

/**
 * db_close - Close and release resources of an open database connection
 * @cpu:	Logical CPU number for per-CPU bookkeeping (opaque)
 * @ras:	RAS events context (opaque)
 *
 * Returns:
 * 0 on success or a negative errno value on failure.
 */
int db_close(unsigned int cpu, struct ras_events *ras);

#else /* HAVE_DB */

static inline int db_backend_enable(const char *name)
{
	return 0;
}

static inline const char *db_list_available_backends(void)
{
	return "";
}

static inline int ras_db_table_register(struct ras_module_ctx *ctx,
					struct db_desc_and_stmt *entry)
{
	return 0;
}

static inline void ras_db_table_unregister(struct ras_module_ctx *ctx)
{
}

static inline int db_bind(const struct db_table_descriptor *db_tab,
			  struct ras_stmt *stmt, int pos, uint64_t value, int len)
{
	return 0;
}

static inline const char *db_get_sql_type(enum db_field_type type, bool is_pk)
{
	return "";
}

static inline int db_eval_stmt(struct ras_stmt *stmt, const char *tab_name)
{
	return 0;
}

static inline int db_create_table(struct ras_db *db,
				  const struct db_table_descriptor *db_tab)
{
	return 0;
}

static inline int db_alter_table(struct ras_db *db, struct ras_stmt **stmt,
				 const struct db_table_descriptor *db_tab)
{
	if (stmt)
		*stmt = NULL;
	return 0;
}

static inline int db_prepare_insert_stmt(struct ras_db *db,
					 struct ras_stmt **stmt,
					 const struct db_table_descriptor *db_tab)
{
	if (stmt)
		*stmt = NULL;
	return 0;
}

static inline int db_create_table_prep_stmt(struct ras_events *ras,
					    struct ras_stmt **stmt,
					    const struct db_table_descriptor *db_tab)
{
	if (stmt)
		*stmt = NULL;
	return 0;
}

static inline int db_exec_sql(struct ras_db *db, const char *sql)
{
	return 0;
}

static inline int db_finalize(struct ras_stmt *stmt)
{
	return 0;
}

static inline int db_cpu_finalize(unsigned int cpu, struct ras_stmt *stmt,
				  const char *name)
{
	return 0;
}

static inline int db_open(struct db_backend *backend, unsigned int cpu,
			  struct ras_events *ras, size_t size_priv)
{
	return 0;
}

static inline int db_close(unsigned int cpu, struct ras_events *ras)
{
	return 0;
}

#endif /* HAVE_DB */

#endif /* RAS_DB_H */
