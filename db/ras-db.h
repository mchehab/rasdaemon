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
 * @name: Column name identifier
 * @type: Data type enumeration for binding logic
 * @is_pk: True if this field is the primary key (affects SQL generation)
 */
struct db_fields {
	const char		*name;
	enum db_field_type	type;
	bool			is_pk;
};

/**
 * struct db_table_descriptor - Metadata describing a database table schema
 * @name: Name of the table in the database
 * @fields: Array of column definitions
 * @num_fields: Total count of columns in the array
 */
struct db_table_descriptor {
	const char			*name;
	const struct db_fields		*fields;
	size_t				num_fields;
};

/* Opaque types to make SQL data structs generic */
struct ras_db;
struct ras_stmt;
struct ras_events;

/**
 * ops_bind_type - Bind a single value based on its type descriptor
 * @stmt: Statement handle provided by the backend
 * @type: The type of data being bound
 * @pos: Parameter position (1-based index)
 * @value: Raw 64-bit integer representation of the value
 * @len: Length for text/blob types, -1 if null-terminated
 */
void db_bind_type(struct ras_stmt *stmt, const enum db_field_type type,
			 const int pos, uint64_t value, int len);

/**
 * ops_bind - Bind a complete set of fields from an event structure
 * @stmt: Statement handle provided by the backend
 * @fields: Table descriptor providing type metadata for binding
 * @pos: Starting position index (1-based)
 * @value: Pointer to raw data buffer containing all field values
 * @len: Length of the buffer (optional, depends on implementation)
 */
void db_bind(struct ras_stmt *stmt, const struct db_fields *fields,
		    const int pos, uint64_t value, int len);


/* TODO: add documentation to those */
const char *db_get_sql_type(enum db_field_type type, bool is_pk);
int db_eval_stmt(struct ras_stmt *stmt, const char *tab_name);
int db_create_table(struct ras_db *db,
			const struct db_table_descriptor *db_tab);
int db_alter_table(struct ras_db *db,
		       struct ras_stmt **stmt,
		       const struct db_table_descriptor *db_tab);
int db_prepare_stmt(struct ras_db *db,
			struct ras_stmt **stmt,
			const struct db_table_descriptor *db_tab);
int db_create_table_prep_stmt(struct ras_events *ras, struct ras_stmt **stmt,
			    const struct db_table_descriptor *db_tab);
int db_finalize(struct ras_stmt *stmt);
int db_cpu_finalize(unsigned int cpu, struct ras_stmt *__stmt, const char *name);
int db_open(unsigned int cpu, struct ras_events *ras, size_t size_priv);
int db_close(unsigned int cpu, struct ras_events *ras);

#endif /* RAS_DB_H */
