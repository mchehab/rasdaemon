/* SPDX-License-Identifier: GPL-2.0 */
#ifndef RAS_DB_REGISTER_H
#define RAS_DB_REGISTER_H

#include <stdbool.h>
#include "db/ras-db.h"

struct ras_db_backend_ops {
	int  (*open)(struct ras_db **db, void *conn_parms,
		     unsigned int cpu);
	int  (*close)(struct ras_db *db, unsigned int cpu);

	const char *(*get_sql_type)(enum db_field_type type, bool is_pk);
	int (*bind_type)(struct ras_stmt *stmt, enum db_field_type type,
			 int pos, uint64_t value, int len);

	int  (*db_exec_sql)(struct ras_db *__db, const char *sql);

	int  (*eval_stmt)(struct ras_stmt *stmt, const char *tab_name);

	int  (*create_table)(struct ras_db *db,
			     const struct db_table_descriptor *tab);
	int  (*alter_table)(struct ras_db *db, struct ras_stmt **stmt,
			    const struct db_table_descriptor *tab);

	int  (*prepare_stmt)(struct ras_db *db, struct ras_stmt **stmt,
			     const struct db_table_descriptor *tab);

	int  (*finalize)(unsigned int cpu, struct ras_stmt *stmt,
			 const char *name);
};

struct ras_db_backend_entry {
	const char *name;
	const struct ras_db_backend_ops *ops;
	void *priv;
	bool allow_remote;

	struct ras_db_backend_entry *next;
};

int db_backend_register(struct ras_db_backend_entry *entry);

#endif /* RAS_DB_REGISTER_H */
