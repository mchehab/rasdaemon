/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2026 Mauro Carvalho Chehab <mchehab+huawei@kernel.org>
 *
 * MySQL / MariaDB backend for rasdaemon.
 *
 * BuildRequires: mysql-devel   (or mariadb-devel)
 * Links:		 -lmysqlclient (or -lmariadb)
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <mysql/mysql.h>

#include "config.h"

#include "core/modules.h"
#include "core/ras-logger.h"

#include "db/ras-db.h"
#include "db/ras-db-backend.h"
#include "db/db-mysql.h"

//#define DEBUG_SQL

void *conn_parms db_sql_get_conn_parms(void)
{
	static struct db_postgresql_conn_params cp;

	cp.connect_timeout = env_or_int("RAS_MYSQL_CONNECT_TIMEOUT", 10);
	cp.port = env_or_int("RAS_MYSQL_PORT", 5432);

	cp.host = env_or("RAS_MYSQL_HOST", NULL);
	cp.user = env_or("RAS_MYSQL_USER", "rasdaemon");
	cp.password = env_or("RAS_MYSQL_PASSWORD", "");
	cp.database = env_or("RAS_MYSQL_DATABASE", "rasdaemon");
	cp.socket = env_or("RAS_MYSQL_SOCKET", NULL);

	cp.use_ssl = env_or_bool("RAS_MYSQL_USE_SSL", false);

	return &cp;
}

static MYSQL *to_db(struct ras_db *__db)
{
	return (MYSQL *)__db;
}


struct mysql_stmt_priv {
	MYSQL_STMT	*stmt;
	unsigned int	n_params;
	MYSQL_BIND	*binds;
};

// TODO: assert that allocs won't return errors

static int db_mysql_open(struct ras_db **__db, void *__conn_parms,
			 unsigned int cpu)
{
	struct db_mysql_conn_params *cp = __conn_parms;
	const char *host, *user, *pass, *database, *sock;
	MYSQL *raw = mysql_init(NULL);
	unsigned int port;
	MYSQL *db = NULL;

	if (!raw) {
		log(TERM, LOG_ERR, "mysql_init failed\n");
		return -ENOMEM;
	}

	if (cp->use_ssl)
		mysql_options(raw, MYSQL_OPT_SSL_MODE,
			      (void *)SSL_MODE_REQUIRED);

	if (cp->connect_timeout)
		mysql_options(raw, MYSQL_OPT_CONNECT_TIMEOUT,
			      (void *)&cp->connect_timeout);

	host = (cp && cp->host) ? cp->host : NULL;
	user = (cp && cp->user) ? cp->user : "root";
	pass = (cp && cp->password) ? cp->password : "";
	database = (cp && cp->database) ? cp->database : NULL;
	port = (cp && cp->port) ? cp->port : 3306;
	sock = (cp && cp->socket) ? cp->socket : NULL;

	db = mysql_real_connect(raw, host, user, pass, database, port, sock, 0);

	if (!db) {
		log(TERM, LOG_ERR,
			"cpu %u: Failed to connect to MySQL: %s\n",
			cpu, mysql_error(raw));
		mysql_close(raw);
		return -1;
	}

	log(TERM, LOG_INFO, "cpu %u: Connected to MySQL (%s)\n",
	    cpu, db->host ? db->host : "local socket");

	*__db = (void *)db;
	return 0;
}

static int db_mysql_close(struct ras_db *__db, unsigned int cpu)
{
	MYSQL *db = to_db(__db);

	if (db)
		mysql_close(db);

	return 0;
}

static const char *db_mysql_get_sql_type(enum db_field_type type, bool is_pk)
{
	switch (type) {
	case DB_TYPE_SERIAL:
		if (is_pk)
			return "BIGINT AUTO_INCREMENT";
		return "BIGINT";
	case DB_TYPE_INT32:
		if (is_pk)
			return "INT PRIMARY KEY";
		return "INT";
	case DB_TYPE_INT64:
		if (is_pk)
			return "BIGINT PRIMARY KEY";
		return "BIGINT";
	case DB_TYPE_TIMESTAMP:
		/* MySQL DATETIME(6) stores ISO-like timestamps */
		if (is_pk)
			return "DATETIME(6) PRIMARY KEY";
		return "DATETIME(6)";
	case DB_TYPE_TEXT:
		if (is_pk)
			return "TEXT PRIMARY KEY";
		return "TEXT";
	case DB_TYPE_BLOB:
		if (is_pk)
			return "BLOB PRIMARY KEY";
		return "BLOB";
	default:
		if (is_pk)
			return "TEXT PRIMARY KEY";
		return "TEXT";
	}
}

static int bind_iso_datetime(MYSQL_BIND *mb, const char *value)
{
	MYSQL_TIME *time;
	struct tm tm = {0};
	char *end;

	end = strptime(value, "%Y-%m-%d %H:%M:%S %z", &tm);
	if (!end)
		return -1;

	time = calloc(1, sizeof(*time));
	if (!time)
		return -1;

	time->year   = tm.tm_year + 1900;
	time->month  = tm.tm_mon + 1;
	time->day    = tm.tm_mday;
	time->hour   = tm.tm_hour;
	time->minute = tm.tm_min;
	time->second = tm.tm_sec;

	time->second_part = 0;
	time->neg = false;
	time->time_type = MYSQL_TIMESTAMP_DATETIME;

	mb->buffer_type = MYSQL_TYPE_DATETIME;
	mb->buffer = time;
	mb->buffer_length = sizeof(*time);

	return 0;
}

static int db_mysql_bind_type(struct ras_stmt *__stmt,
			      const enum db_field_type type,
			      const int pos, uint64_t value, int len)
{
	struct mysql_stmt_priv *priv = (struct mysql_stmt_priv *)__stmt;
	unsigned int idx = (unsigned int)pos - 1;
	MYSQL_BIND *mb = &priv->binds[idx];

	memset(mb, 0, sizeof(*mb));

	switch (type) {
	case DB_TYPE_SERIAL:
		/* In practice, should never happen */
		mb->buffer_type = MYSQL_TYPE_NULL;
		mb->is_null = malloc(sizeof(bool));
		if (!mb->is_null) {
			log(TERM, LOG_ERR,
			    "Failed to allocate memory for NULL\n");
			return -1;
		}
		*(bool *)mb->is_null = true;
		return 0;

	case DB_TYPE_INT32:
		mb->buffer_type = MYSQL_TYPE_LONG;
		mb->buffer_length = sizeof(int32_t);
		mb->buffer = malloc(mb->buffer_length);
		if (!mb->buffer) {
			log(TERM, LOG_ERR,
			    "Failed to allocate memory for INT\n");
			return -1;
		}
		mb->length = &mb->buffer_length;
		*((int32_t *)mb->buffer) = value;

		return 0;

	case DB_TYPE_INT64:
		mb->buffer_type = MYSQL_TYPE_LONGLONG;
		mb->buffer_length = sizeof(int64_t);
		mb->buffer = malloc(mb->buffer_length);
		if (!mb->buffer) {
			log(TERM, LOG_ERR,
			    "Failed to allocate memory for BIG INT\n");
			return -1;
		}
		mb->length = &mb->buffer_length;
		*(int64_t *)mb->buffer = (int64_t)value;
		return 0;

	case DB_TYPE_TIMESTAMP:
		return bind_iso_datetime(mb, (const char *)(uintptr_t)value);

	case DB_TYPE_TEXT:
		mb->buffer_type = MYSQL_TYPE_VAR_STRING;
		break;

	default:
	case DB_TYPE_BLOB:
		mb->buffer_type = MYSQL_TYPE_BLOB;
		break;
	}

	if (!value) {
		mb->is_null = malloc(sizeof(bool));
		if (!mb->buffer) {
			log(TERM, LOG_ERR,
			    "Failed to allocate memory for NULL\n");
			return -1;
		}
		*(bool *)mb->is_null = true;
	} else {
		const char *str = (const char *)value;

		if (len < 0)
			len = strlen(str);

		mb->buffer_length = len;
		mb->length = &mb->buffer_length;

		if (type == DB_TYPE_BLOB) {
			mb->buffer = malloc(len);
			if (!mb->buffer) {
				log(TERM, LOG_ERR,
				"Failed to allocate memory for BLOB\n");
				return -1;
			}

			memcpy(mb->buffer, str, len);
		} else {
			mb->buffer = malloc(len + 1);
			if (!mb->buffer) {
				log(TERM, LOG_ERR,
				"Failed to allocate memory for TEXT\n");
				return -1;
			}

			strncpy(mb->buffer, str, len);
			*(char *)(mb->buffer + len) = '\0';
		}
	}
	return 0;
}

static int db_mysql_exec_sql(struct ras_db *__db, const char *sql)
{
	MYSQL *db = to_db(__db);
	int rc;

#ifdef DEBUG_SQL
	log(TERM, LOG_INFO, "SQL: %s\n", sql);
	ras_logger_flush();
#endif

	rc = mysql_query(db, sql);
	if (rc) {
		log(TERM, LOG_ERR,
			"Failed to exec '%s': %s\n", sql, mysql_error(db));
		return -1;
	}

	return 0;
}


static int db_mysql_create_table(struct ras_db *__db,
				 const struct db_table_descriptor *db_tab)
{
	char sql[2048], *p = sql, *end = sql + sizeof(sql) - 1;
	const struct db_fields *field;
	const char *type;
	int i;

	p += snprintf(p, end - p, "CREATE TABLE IF NOT EXISTS %s (",
			  db_tab->name);

	for (i = 0; i < (int)db_tab->num_fields; i++) {
		field = &db_tab->fields[i];
		type = db_mysql_get_sql_type(field->type, field->is_pk);

		p += snprintf(p, end - p, "%s %s", field->name, type);

		if (i < (int)db_tab->num_fields - 1)
			p += snprintf(p, end - p, ", ");
	}

	/* MySQL requires an explicit PRIMARY KEY clause if SERIAL was used */
	for (i = 0; i < (int)db_tab->num_fields; i++) {
		field = &db_tab->fields[i];
		if (field->is_pk &&
			field->type == DB_TYPE_SERIAL) {
			p += snprintf(p, end - p, ", PRIMARY KEY (%s)",
				      field->name);
			break;
		}
	}

	p += snprintf(p, end - p, ") ENGINE=InnoDB");
	*end = '\0';

	return db_mysql_exec_sql(__db, sql);
}

static int db_mysql_alter_table(struct ras_db *__db,
				struct ras_stmt **__stmt,
				const struct db_table_descriptor *db_tab)
{
	MYSQL *db = to_db(__db);
	MYSQL_RES *res = NULL;
	MYSQL_ROW row;
	unsigned long *lengths = NULL;
	char sql[1024];
	const struct db_fields *field;
	int i, found, rc = 0;

	snprintf(sql, sizeof(sql),
		 "SELECT COLUMN_NAME FROM INFORMATION_SCHEMA.COLUMNS "
		 "WHERE TABLE_NAME = '%s'",
		 db_tab->name);

	res = mysql_query(db, sql) == 0 ? mysql_store_result(db) : NULL;
	if (!res) {
		log(TERM, LOG_ERR, "Failed to query columns of %s: %s\n",
		    db_tab->name, mysql_error(db));
		return -1;
	}

	for (i = 0; i < (int)db_tab->num_fields; i++) {
		field = &db_tab->fields[i];
		found = 0;

		/* scan the result set */
		mysql_data_seek(res, 0);
		while ((row = mysql_fetch_row(res))) {
			if (!strcmp(field->name, row[0])) {
				found = 1;
				break;
			}
		}

		if (!found) {
			const char *type = db_mysql_get_sql_type(
				field->type, field->is_pk);

			snprintf(sql, sizeof(sql),
				 "ALTER TABLE %s ADD COLUMN %s %s",
				 db_tab->name, field->name, type);

			if (mysql_query(db, sql)) {
				log(TERM, LOG_ERR,
					"ALTER TABLE %s ADD %s: %s\n",
					db_tab->name, field->name,
					mysql_error(db));
				rc = -1;
			}
		}
	}

	if (lengths)
		free(lengths);
	mysql_free_result(res);

	return rc;
}

static int db_mysql_prepare_insert_stmt(struct ras_db *__db,
					struct ras_stmt **__stmt,
					const struct db_table_descriptor *db_tab)
{
	char sql[2048], *p = sql, *end;
	struct mysql_stmt_priv *priv;
	const struct db_fields *field;
	unsigned int n_params = 0;
	MYSQL *db = to_db(__db);
	MYSQL_STMT *ms;
	unsigned int i;

	end = sql + sizeof(sql) - 1;

	p += snprintf(p, end - p, "INSERT INTO %s (", db_tab->name);

	for (i = 0; i < db_tab->num_fields; i++) {
		field = &db_tab->fields[i];
		p += snprintf(p, end - p, "%s", field->name);
		if (i < db_tab->num_fields - 1)
			p += snprintf(p, end - p, ", ");
	}

	p += snprintf(p, end - p, ") VALUES (");

	for (i = 0; i < db_tab->num_fields; i++) {
		field = &db_tab->fields[i];
		if (field->type == DB_TYPE_SERIAL) {
			p += snprintf(p, end - p, "DEFAULT");
		} else {
			n_params++;
			p += snprintf(p, end - p, "?");
		}
		if (i < db_tab->num_fields - 1)
			p += snprintf(p, end - p, ", ");
	}
	p += snprintf(p, end - p, ")");
	*end = '\0';

#ifdef DEBUG_SQL
	log(TERM, LOG_INFO, "SQL: %s\n", sql);
	ras_logger_flush();
#endif

	ms = mysql_stmt_init(db);

	if (!ms) {
		log(TERM, LOG_ERR,
		    "mysql_stmt_init failed: %s\n", mysql_error(db));
		return -1;
	}

	if (mysql_stmt_prepare(ms, sql, (unsigned int)strlen(sql))) {
		log(TERM, LOG_ERR,
		    "mysql_stmt_prepare(%s) failed: %s\n",
		    sql, mysql_stmt_error(ms));
		mysql_stmt_close(ms);
		return -1;
	}

	priv = calloc(1, sizeof(*priv));
	if (!priv) {
		log(TERM, LOG_ERR, "No memory for MySQL stmt priv\n");
		mysql_stmt_close(ms);
		return -ENOMEM;
	}

	priv->stmt = ms;
	priv->n_params = n_params;
	priv->binds = calloc(n_params, sizeof(MYSQL_BIND));

	*__stmt = (void *)priv;

	log(TERM, LOG_INFO, "Recording %s events (mysql)\n", db_tab->name);
	return 0;
}

static void db_mysql_free_stmt(struct mysql_stmt_priv *priv)
{
	for (int i = 0; i < priv->n_params; i++) {
		if (priv->binds[i].buffer) {
			free(priv->binds[i].buffer);
			priv->binds[i].buffer = NULL;
		}
		if (priv->binds[i].is_null) {
			free(priv->binds[i].is_null);
			priv->binds[i].is_null = NULL;
		}
	}
}

static int db_mysql_eval_stmt(struct ras_stmt *__stmt, const char *tab_name)
{
	struct mysql_stmt_priv *priv = (struct mysql_stmt_priv *)__stmt;
	MYSQL_STMT *ms = priv->stmt;
	int rc1, rc2;

	rc1 = mysql_stmt_bind_param(ms, priv->binds);

	if (!rc1)
		rc2 = mysql_stmt_execute(ms);

	db_mysql_free_stmt(priv);

	if (rc1) {
		log(TERM, LOG_ERR, "mysql_stmt_bind_param (%s): %s\n",
		    tab_name, mysql_stmt_error(ms));
		ras_logger_flush();
		return -1;
	}

	if (rc2) {
		log(TERM, LOG_ERR, "mysql_stmt_execute (%s): %s\n",
		    tab_name, mysql_stmt_error(ms));
		ras_logger_flush();
		return -1;
	}

	/* reset for next use */
	mysql_stmt_reset(ms);

	return 0;
}

static int db_mysql_finalize(unsigned int cpu,
				 struct ras_stmt *__stmt, const char *name)
{
	struct mysql_stmt_priv *priv = (struct mysql_stmt_priv *)__stmt;

	if (!priv)
		return 0;

	db_mysql_free_stmt(priv);

	mysql_stmt_close(priv->stmt);
	free(priv);

	return 0;
}

/*
 * Backend registration
 */

static const struct ras_db_backend_ops mysql_backend_ops = {
	.open			= db_mysql_open,
	.close			= db_mysql_close,
	.get_conn_parms		= db_mysql_get_conn_parms,

	.get_sql_type		= db_mysql_get_sql_type,
	.bind_type		= db_mysql_bind_type,

	.db_exec_sql		= db_mysql_exec_sql,

	.eval_stmt		= db_mysql_eval_stmt,
	.create_table		= db_mysql_create_table,
	.alter_table		= db_mysql_alter_table,
	.prepare_stmt		= db_mysql_prepare_insert_stmt,
	.finalize		= db_mysql_finalize,
};

static struct ras_db_backend_entry mysql_backend_entry = {
	.name		= "mysql",
	.ops		= &mysql_backend_ops,
	.allow_remote	= true,
};

static int mysql_module_init(const char *name, struct ras_events *ras,
			     void **priv)
{
	int ret;

	ret = db_backend_register(&mysql_backend_entry);
	if (ret != 0) {
		log(TERM, LOG_ERR, "Failed to init MySQL backend: %d\n", ret);
	} else {
		log(TERM, LOG_INFO, "MySQL DB backend initialized.\n");
	}

	return ret;
}

const struct ras_module_entry db_mysql_module = {
	.name = "db-mysql",
	.init = mysql_module_init,
	.level = DB_MODULE,
};

__attribute__((constructor)) void mysql_register(void)
{
	int ret;

	ret = module_register(&db_mysql_module);
	if (ret != 0) {
		log(TERM, LOG_ERR, "Failed to register MySQL module: %d\n",
		    ret);
	} else {
		log(TERM, LOG_INFO,
		    "MySQL backend registered successfully.\n");
	}
}
