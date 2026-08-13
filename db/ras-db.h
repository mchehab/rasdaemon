/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2026 Mauro Carvalho Chehab <mchehab+huawei@kernel.org>
 */

#ifndef RAS_DB_H
#define RAS_DB_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * BuildRequires: sqlite-devel
 */

/* #define DEBUG_SQL 1 */

/* Opaque types to make SQL data structs generic */
struct ras_db;
struct ras_stmt;
struct ras_events;

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
 * struct db_fields - Definition of a single column in a table descriptor
 * @name:	Column name identifier
 * @type:	Data type enumeration for binding logic
 * @is_pk:	True if this field is the primary key (affects SQL generation)
 */
struct db_fields {
	const char		*name;
	enum db_field_type	type;
	bool			is_pk;
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
 * struct db_backend - Specify what DB backend will be used
 * @backend:	Name of backend driver
 * @conn_parms:	Connection parameters (specific to each type of DB
 */
struct db_backend {
	const char *name;
	void *conn_parms;
};

/**
 * ops_bind_type - Bind a single value based on its type descriptor
 * @stmt:	Statement handle provided by the backend
 * @type:	The type of data being bound
 * @pos:	Parameter position (1-based index)
 * @value:	Raw 64-bit integer representation of the value
 * @len:	Length for text/blob types, -1 if null-terminated
 */
void db_bind_type(struct ras_stmt *stmt, const enum db_field_type type,
		  const int pos, uint64_t value, int len);

/**
 * ops_bind - Bind fields from an event structure using fields definition
 * @stmt:	Statement handle provided by the backend
 * @fields:	Table descriptor providing type metadata for binding
 * @pos:	Starting position index (1-based)
 * @value:	Pointer to raw data buffer containing all field values
 * @len:	Length of the buffer (optional, depends on implementation)
 */
void db_bind(struct ras_stmt *stmt, const struct db_fields *fields,
	     const int pos, uint64_t value, int len);

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

#endif /* RAS_DB_H */
