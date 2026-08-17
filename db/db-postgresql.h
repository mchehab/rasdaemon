/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2026 Mauro Carvalho Chehab <mchehab+huawei@kernel.org>
 */

#ifndef DB_POSTGRESQL_H
#define DB_POSTGRESQL_H

#include <stdbool.h>

/**
 * struct db_postgresql_conn_params - PostgreSQL connection parameters
 * @host:	Hostname or IP. NULL or "localhost" for local Unix socket.
 * @port:	TCP port (default 5432). Ignored for local socket.
 * @user:	Username (default "rasdaemon").
 * @password:	Password (default NULL / empty).
 * @schema:	Schema to be used (default "rasdaemon").
 * @database:	Database name (default "rasdaemon").
 * @socket_dir:	Unix socket directory for local connections
 *		(default "/var/run/postgresql").
 */
struct db_postgresql_conn_params {
	const char *host;
	unsigned int port;
	const char *user;
	const char *password;
	const char *schema;
	const char *database;
	const char *socket_dir;

	unsigned int connect_timeout;

	bool use_ssl;
	const char *sslmode;
};

void postgresql_register_backend(void);

#endif /* DB_POSTGRESQL_H */
