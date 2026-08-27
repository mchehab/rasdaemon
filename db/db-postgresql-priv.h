/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2026 Mauro Carvalho Chehab <mchehab+huawei@kernel.org>
 */

#ifndef DB_POSTGRESQL_PRIV_H
#define DB_POSTGRESQL_PRIV_H

#include <stdbool.h>

/*
 * This should never be used anywhere, except at postgresql unit test
 *
 * The goal of this struct is to allow embedding the schema inside the
 * DB to allow create/alter table to use the right schema.
 */

struct pg_conn_priv {
	PGconn *conn;
	const char *schema;
};

#endif /* DB_POSTGRESQL_PRIV_H */
