/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2026 Mauro Carvalho Chehab <mchehab+huawei@kernel.org>
 */

#ifndef DB_POSTGRESQL_H
#define DB_POSTGRESQL_H

#include <stdbool.h>

/**
 * struct db_postgresql_conn_params - PostgreSQL connection parameters
 * @host:	Hostname or IP. NULL or empty for a local Unix socket.
 * @port:	TCP port (default 5432). Ignored for local socket.
 * @user:	Username (default "rasdaemon").
 * @password:	Password (default NULL / empty).
 * @schema:	Schema to be used (default "rasdaemon").
 * @database:	Database name (default "rasdaemon").
 */
struct db_postgresql_conn_params {
	const char *host;
	unsigned int port;
	const char *user;
	const char *password;
	const char *schema;
	const char *database;

	unsigned int connect_timeout;

	bool use_ssl;
	const char *sslmode;
};

void postgresql_register_backend(void);

#endif /* DB_POSTGRESQL_H */
