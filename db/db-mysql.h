/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2026 Mauro Carvalho Chehab <mchehab+huawei@kernel.org>
 */

#ifndef DB_MYSQL_H
#define DB_MYSQL_H

#include <stdbool.h>

/**
 * struct db_mysql_conn_params - MySQL/MariaDB connection parameters
 * @host:	Hostname or IP. NULL or empty for a local Unix socket.
 * @port:	TCP port (default 3306). Ignored for local socket.
 * @user:	Username (default "rasdaemon").
 * @password:	Password (default NULL / empty).
 * @database:	Database name (default "rasdaemon").
 * @socket:	Unix socket path for local connections
 *		(default "/var/lib/mysql/mysql.sock").
 */
struct db_mysql_conn_params {
	const char *host;
	unsigned int port;
	const char *user;
	const char *password;
	const char *database;
	const char *socket;

	unsigned int connect_timeout;

	bool use_ssl;
};

void mysql_register_backend(void);

#endif /* DB_MYSQL_H */
