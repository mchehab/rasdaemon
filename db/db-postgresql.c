/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2026 Mauro Carvalho Chehab <mchehab+huawei@kernel.org>
 *
 * PostgreSQL backend for rasdaemon.
 *
 * BuildRequires: postgresql-devel   (libpq)
 * Links:		 -lpq
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <libpq-fe.h>

#include "config.h"

#include "core/modules.h"
#include "core/ras-logger.h"

#include "db/ras-db.h"
#include "db/ras-db-backend.h"
#include "db/db-postgresql.h"
#include "db/db-postgresql-priv.h"

//#define DEBUG_SQL

struct pg_stmt_priv {
	PGconn	*conn;
	char	*stmt_name;
	unsigned int n_params;

	bool is_blob;

	char **values;
	int *lengths;
};

static void *db_pg_get_conn_parms(void)
{
	static struct db_postgresql_conn_params cp;

	cp.connect_timeout = env_or_int("RAS_PG_CONNECT_TIMEOUT", 10);
	cp.port = env_or_int("RAS_PG_PORT", 5432);

	cp.host = env_or("RAS_PG_HOST", NULL);
	cp.user = env_or("RAS_PG_USER", "rasdaemon");
	cp.password = env_or("RAS_PG_PASSWORD", "");
	cp.schema = env_or("RAS_PG_SCHEMA", "rasdaemon");
	cp.database = env_or("RAS_PG_DATABASE", "rasdaemon");
	cp.sslmode = env_or("RAS_PG_SSL_MODE", NULL);

	cp.use_ssl = env_or_bool("RAS_PG_USE_SSL", false);

	return &cp;
}

static int db_pg_open(struct ras_db **__db, void *__conn_parms,
			  unsigned int cpu)
{
	char conninfo[1024], *p, *end = conninfo + sizeof(conninfo);
	struct db_postgresql_conn_params *cp = __conn_parms;
	struct pg_conn_priv *conn_priv;
	const char *schema;
	PGconn *conn;

	conn_priv = calloc(1, sizeof(struct pg_conn_priv));
	if (!conn_priv) {
		log(TERM, LOG_ERR,
		    "Failed to allocate memory for PostgreSQL connection\n");
		return -1;
	}

	/* Build the libpq connection string */
	if (cp) {
		p = conninfo;

		if (cp->host && *cp->host) {
			p += snprintf(p, end - p, "host=%s ", cp->host);
			if (cp->port)
				p += snprintf(p, end - p, "port=%hu ", cp->port);
		}

		if (!cp->user || !*cp->user)
			cp->user = "rasdaemon";

		p += snprintf(p, end - p, "user=%s ", cp->user);

		if (cp->password && *cp->password)
			p += snprintf(p, end - p, "password=%s ", cp->password);

		if (cp->schema && *cp->schema)
			schema = cp->schema;
		else
			schema = "rasdaemon";

		if (cp->database && *cp->database)
			p += snprintf(p, end - p, "dbname=%s ", cp->database);

		if (cp->use_ssl) {
			if (cp->sslmode && *cp->sslmode)
				p += snprintf(p, end - p, "sslmode=%s ", cp->sslmode);
			else
				p += snprintf(p,  end - p, "sslmode=require ");
		}

		if (cp->connect_timeout)
			p += snprintf(p, end - p, "connect_timeout=%u ",
					cp->connect_timeout);
	}
	*p = '\0';

	conn = PQconnectdb(conninfo);

	if (PQstatus(conn) != CONNECTION_OK) {
		log(TERM, LOG_ERR,
			"cpu %u: PostgreSQL connection failed: %s\n",
			cpu, PQerrorMessage(conn));
		PQfinish(conn);
		free(conn_priv);
		return -1;
	}

	conn_priv->conn = conn;
	conn_priv->schema = schema;

	log(TERM, LOG_INFO,
		"cpu %u: Connected to PostgreSQL at %s, schema: %s\n",
		cpu, PQhost(conn), schema);

	*__db = (void *)conn_priv;
	return 0;
}

static int db_pg_close(struct ras_db *__db, unsigned int cpu)
{
	struct pg_conn_priv *conn_priv = (void *)__db;
	PGconn *conn = conn_priv->conn;
	int rc = 0;

	if (conn) {
		ConnStatusType st = PQstatus(conn);

		if (st == CONNECTION_OK || st == CONNECTION_BAD)
			PQfinish(conn);

		if (st != CONNECTION_OK && st != CONNECTION_BAD) {
			log(TERM, LOG_ERR,
				"cpu %u: Unexpected PGconn state %d\n",
				cpu, (int)st);
			rc = -1;
		}
	}

	return rc;
}

static const char *db_pg_get_sql_type(enum db_field_type type, bool is_pk)
{
	switch (type) {
	case DB_TYPE_SERIAL:
		/* PostgreSQL uses SERIAL / BIGSERIAL for identity */
		if (is_pk)
			return "BIGSERIAL";
		return "BIGINT";
	case DB_TYPE_INT32:
		if (is_pk)
			return "INTEGER PRIMARY KEY";
		return "INTEGER";
	case DB_TYPE_INT64:
		if (is_pk)
			return "BIGINT PRIMARY KEY";
		return "BIGINT";
	case DB_TYPE_TIMESTAMP:
		if (is_pk)
			return "TIMESTAMPTZ PRIMARY KEY";
		return "TIMESTAMPTZ";
	case DB_TYPE_TEXT:
		if (is_pk)
			return "TEXT PRIMARY KEY";
		return "TEXT";
	case DB_TYPE_BLOB:
	default:
		if (is_pk)
			return "BYTEA PRIMARY KEY";
		return "BYTEA";
	}
}

static int db_pg_bind_type(struct ras_stmt *__stmt,
			   const enum db_field_type type,
			   const int pos, uint64_t value, int len)
{
	struct pg_stmt_priv *priv = (struct pg_stmt_priv *)__stmt;
	unsigned int idx = (unsigned int)pos - 1;
	PGconn	*conn = priv->conn;
	size_t encoded_len;
	const char *str;
	char *buf;

	if (idx >= priv->n_params) {
		log(TERM, LOG_ERR,
			"pg_bind_type: pos %d out of range (n=%u)\n",
			pos, priv->n_params);
		return -1;
	}

	priv->is_blob = false;

	/* Just in case, to avoid memory leaks */
	if (priv->values[idx]) {
		free(priv->values[idx]);
		priv->values[idx] = NULL;
	}

	switch (type) {
	case DB_TYPE_SERIAL:
		priv->values[idx] = NULL;
		priv->lengths[idx] = 0;
		return 0;

	case DB_TYPE_INT32:
		asprintf(&buf, "%d", (int32_t)value);
		if (!buf) {
			log(TERM, LOG_ERR,"Failed to allocate memory for INT32\n");
			return -1;
		}

		priv->values[idx] = buf;
		priv->lengths[idx] = strlen(buf);
		return 0;

	case DB_TYPE_INT64:
		asprintf(&buf, "%lld", (long long)value);
		if (!buf) {
			log(TERM, LOG_ERR,"Failed to allocate memory for INT64\n");
			return -1;
		}
		priv->values[idx] = buf;
		priv->lengths[idx] = strlen(buf);
		return 0;

	case DB_TYPE_TIMESTAMP:
	case DB_TYPE_TEXT:
	case DB_TYPE_BLOB:
	default:
		break;
	}

	if (!value) {
		priv->values[idx] = NULL;
		priv->lengths[idx] = 0;
		return 0;
	}

	str = (const char *)value;
	if (len < 0)
		len = strlen(str);

	priv->lengths[idx] = len;

	if (type == DB_TYPE_BLOB) {
		buf = (char *)PQescapeByteaConn(conn, (unsigned char *)str,
						len, &encoded_len);
		if (!buf) {
			log(TERM, LOG_ERR,"Failed to allocate memory for BLOB\n");
			return -1;
		}

		priv->is_blob = true;
		priv->values[idx] = buf;
		priv->lengths[idx] = encoded_len;
		return 0;
	}

	buf = malloc(len + 1);
	if (!buf) {
		log(TERM, LOG_ERR,
		"Failed to allocate memory for TEXT\n");
		return -1;
	}

	memcpy(buf, str, len);
	buf[len] = '\0';

	priv->values[idx] = buf;
	priv->lengths[idx] = len;
	return 0;
}

static int db_pg_exec_sql(struct ras_db *__db, const char *sql)
{
	struct pg_conn_priv *conn_priv = (void *)__db;
	PGconn *conn = conn_priv->conn;
	PGresult *res;
	int rc = 0;

#ifdef DEBUG_SQL
	log(TERM, LOG_INFO, "SQL: %s\n", sql);
	ras_logger_flush();
#endif

	res = PQexec(conn, sql);
	if (PQresultStatus(res) != PGRES_COMMAND_OK) {
		log(TERM, LOG_ERR, "Failed to exec '%s': %s\n",
		    sql, PQresultErrorMessage(res));
		rc = -1;
	}
	PQclear(res);

	return rc;
}


static int db_pg_create_table(struct ras_db *__db,
			      const struct db_table_descriptor *db_tab)
{
	struct pg_conn_priv *conn_priv = (void *)__db;
	char sql[2048], *p = sql, *end = sql + sizeof(sql) - 1;
	const struct db_fields *field;
	const char *type;
	int i;

	p += snprintf(p, end - p, "CREATE TABLE IF NOT EXISTS %s.%s (",
		      conn_priv->schema, db_tab->name);

	for (i = 0; i < (int)db_tab->num_fields; i++) {
		field = &db_tab->fields[i];
		type = db_pg_get_sql_type(field->type, field->is_pk);

		p += snprintf(p, end - p, "%s %s", field->name, type);

		if (i < (int)db_tab->num_fields - 1)
			p += snprintf(p, end - p, ", ");
	}

	/*
	 * For SERIAL / BIGSERIAL we need an explicit PRIMARY KEY clause
	 * (SERIAL already implies NOT NULL + a sequence).
	 */
	for (i = 0; i < (int)db_tab->num_fields; i++) {
		field = &db_tab->fields[i];
		if (field->is_pk && field->type == DB_TYPE_SERIAL) {
			p += snprintf(p, end - p, ", PRIMARY KEY (%s)",
				      field->name);
			break;
		}
	}

	p += snprintf(p, end - p, ")");
	*end = '\0';

	return db_pg_exec_sql(__db, sql);
}

static int db_pg_alter_table(struct ras_db *__db,
			     struct ras_stmt **__stmt,
			     const struct db_table_descriptor *db_tab)
{
	struct pg_conn_priv *conn_priv = (void *)__db;
	PGconn *conn = conn_priv->conn;
	char sql[512];
	PGresult *res;
	const struct db_fields *field;
	int i, nf, found, rc = 0;

	snprintf(sql, sizeof(sql),
		 "SELECT column_name FROM information_schema.columns "
		 "WHERE table_name = '%s.%s'",
		 conn_priv->schema, db_tab->name);

	res = PQexec(conn, sql);
	if (PQresultStatus(res) != PGRES_TUPLES_OK) {
		log(TERM, LOG_ERR,
			"Failed to query columns of %s: %s\n",
			db_tab->name, PQresultErrorMessage(res));
		PQclear(res);
		return -1;
	}

	nf = PQntuples(res);

	for (i = 0; i < (int)db_tab->num_fields; i++) {
		field = &db_tab->fields[i];
		found = 0;

		for (int r = 0; r < nf; r++) {
			if (!strcmp(field->name, PQgetvalue(res, r, 0))) {
				found = 1;
				break;
			}
		}

		if (!found) {
			const char *type = db_pg_get_sql_type(
				field->type, field->is_pk);

			snprintf(sql, sizeof(sql),
				 "ALTER TABLE %s.%s ADD COLUMN %s %s",
				 conn_priv->schema, db_tab->name,
				 field->name, type);

			{
				PGresult *r2 = PQexec(conn, sql);

				if (PQresultStatus(r2) != PGRES_COMMAND_OK) {
					log(TERM, LOG_ERR,
					    "ALTER TABLE %s ADD %s: %s\n",
					    db_tab->name, field->name,
					    PQresultErrorMessage(r2));
					rc = -1;
				}
				PQclear(r2);
			}
		}
	}

	PQclear(res);
	return rc;
}

static int db_pg_prepare_insert_stmt(struct ras_db *__db,
				     struct ras_stmt **__stmt,
				     const struct db_table_descriptor *db_tab)
{
	struct pg_conn_priv *conn_priv = (void *)__db;
	PGconn *conn = conn_priv->conn;
	char sql[2048], *p = sql, *end = sql + sizeof(sql) - 1;
	char stmt_name[128];
	const struct db_fields *field;
	unsigned int n_parms = 0;
	struct pg_stmt_priv *priv;
	int status, idx;
	unsigned int i;
	PGresult *res;

	for (i = 0; i < db_tab->num_fields; i++) {
		if (db_tab->fields[i].type != DB_TYPE_SERIAL)
			n_parms++;
	}

	/* unique statement name: "ins_<tabname>" */
	snprintf(stmt_name, sizeof(stmt_name), "ins_%s", db_tab->name);

	p = sql;
	p += snprintf(p, end - p, "INSERT INTO %s (", db_tab->name);
	for (i = 0; i < db_tab->num_fields; i++) {
		field = &db_tab->fields[i];
		p += snprintf(p, end - p, "%s", field->name);
		if (i < db_tab->num_fields - 1)
			p += snprintf(p, end - p, ", ");
	}
	p += snprintf(p, end - p, ") VALUES (");

	idx = 1;
	for (i = 0; i < db_tab->num_fields; i++) {
		field = &db_tab->fields[i];
		if (field->type == DB_TYPE_SERIAL)
			p += snprintf(p, end - p, "DEFAULT");
		else
			p += snprintf(p, end - p, "$%u", idx++);

		if (i < db_tab->num_fields - 1)
			p += snprintf(p, end - p, ", ");
	}

	p += snprintf(p, end - p, ")");
	*end = '\0';

#ifdef DEBUG_SQL
	log(TERM, LOG_INFO, "SQL: %s\n", sql);
	ras_logger_flush();
#endif

	res = PQprepare(conn, stmt_name, sql, n_parms, NULL);
	if (!res) {
		log(TERM, LOG_ERR,
			"PQprepare failed for %s\n", db_tab->name);
		return -1;
	}

	status = PQresultStatus(res);
	PQclear(res);

	if (status != PGRES_COMMAND_OK) {
		log(TERM, LOG_ERR,
			"PQprepare(%s) failed\n", sql);
		return -1;
	}

	priv = calloc(1, sizeof(*priv));
	if (!priv) {
		log(TERM, LOG_ERR,
			"No memory for PostgreSQL stmt priv\n");
		return -ENOMEM;
	}

	priv->values = calloc(n_parms, sizeof(char *));
	if (!priv) {
		log(TERM, LOG_ERR,
			"No memory for PostgreSQL stmt priv\n");
		return -ENOMEM;
	}

	priv->lengths = calloc(n_parms, sizeof(int *));
	if (!priv) {
		log(TERM, LOG_ERR,
			"No memory for PostgreSQL stmt priv\n");
		return -ENOMEM;
	}

	priv->conn = conn;
	priv->stmt_name = strdup(stmt_name);
	priv->n_params = n_parms;

	*__stmt = (void *)priv;

	log(TERM, LOG_INFO, "Recording %s events (postgresql)\n",
		db_tab->name);
	return 0;
}

static void db_pg_free_stmt(struct pg_stmt_priv *priv)
{
	for (int i = 0; i < priv->n_params; i++) {
		if (priv->values[i]) {
			if (priv->is_blob)
				PQfreemem(priv->values[i]);
			else
				free(priv->values[i]);

			priv->values[i] = NULL;
		}
	}
}


static int db_pg_eval_stmt(struct ras_stmt *__stmt, const char *tab_name)
{
	struct pg_stmt_priv *priv = (struct pg_stmt_priv *)__stmt;
	PGconn *conn = priv->conn;
	PGresult *res;
	int rc = 0;

	res = PQexecPrepared(conn, priv->stmt_name, priv->n_params,
			     (const char * const*)priv->values,
			     priv->lengths, NULL, 0);

	if (!res) {
		log(TERM, LOG_ERR,
			"PQexecPrepared (%s) failed: %s\n",
			tab_name, PQerrorMessage(conn));
		return -1;
	}

	if (PQresultStatus(res) != PGRES_COMMAND_OK &&
		PQresultStatus(res) != PGRES_TUPLES_OK) {
		log(TERM, LOG_ERR,
			"PQexecPrepared (%s): %s\n",
			tab_name, PQresultErrorMessage(res));
		rc = -1;
	}

	PQclear(res);
	db_pg_free_stmt(priv);

	return rc;
}

static int db_pg_finalize(unsigned int cpu,
			  struct ras_stmt *__stmt, const char *name)
{
	struct pg_stmt_priv *priv = (struct pg_stmt_priv *)__stmt;

	if (!priv)
		return 0;

	db_pg_free_stmt(priv);
	free(priv->stmt_name);
	free(priv->values);
	free(priv->lengths);
	free(priv);

	return 0;
}

/*
 * Backend registration
 */

static const struct ras_db_backend_ops pg_backend_ops = {
	.open			= db_pg_open,
	.close			= db_pg_close,
	.get_conn_parms		= db_pg_get_conn_parms,

	.get_sql_type		= db_pg_get_sql_type,
	.bind_type		= db_pg_bind_type,

	.db_exec_sql		= db_pg_exec_sql,

	.eval_stmt		= db_pg_eval_stmt,
	.create_table		= db_pg_create_table,
	.alter_table		= db_pg_alter_table,
	.prepare_stmt		= db_pg_prepare_insert_stmt,
	.finalize		= db_pg_finalize,
};

static struct ras_db_backend_entry pg_backend_entry = {
	.name		= "postgresql",
	.ops		= &pg_backend_ops,
	.allow_remote	= true,
};

static int pg_init(const char *name, struct ras_events *ras, void **priv)
{
	int ret;

	ret = db_backend_register(&pg_backend_entry);
	if (ret != 0)
		log(TERM, LOG_ERR,
			"Failed to init PostgreSQL backend: %d\n", ret);

	return ret;
}

const struct ras_module_entry db_postgresql_module = {
	.name = "db-postgresql",
	.init = pg_init,
	.level = DB_MODULE,
	.postpone_init = true,
};

__attribute__((constructor)) void pg_register(void)
{
	int ret;

	ret = module_register(&db_postgresql_module);
	if (ret != 0)
		log(TERM, LOG_ERR,
		    "Failed to register PostgreSQL module: %d\n", ret);
}
