# SPDX-License-Identifier: GPL-2.0

"""
Database abstraction layer for rasdaemon.

This module provides a generic database interface using SQLAlchemy,
supporting SQLite, MySQL/MariaDB, and PostgreSQL backends.
"""

import os
import sys
import logging
from typing import Optional, List, Dict, Any

logger = logging.getLogger(__name__)

# Check if SQLAlchemy is available
import sqlalchemy
from sqlalchemy import create_engine, text, Column, Integer, String
from sqlalchemy import Float, Text, DateTime, Boolean, MetaData
from sqlalchemy import Table, select, insert, delete, update
from sqlalchemy.exc import OperationalError, ProgrammingError

# Check if psycopg2 is available (for PostgreSQL)
try:
    import psycopg2

    HAS_PSYCOPG2 = True
except ImportError:
    HAS_PSYCOPG2 = False

# Check if mysqlclient is available (for MySQL)
try:
    import mysqlclient

    HAS_MYSQLCLIENT = True
except ImportError:
    HAS_MYSQLCLIENT = False


class RasDatabase:
    """Generic database abstraction layer using SQLAlchemy."""

    def __init__(self, db_backend: str, **kwargs):
        """
        Initialize the database connection.

        Args:
            db_backend: Database backend type (sqlite3, mysql, postgresql).
            **kwargs: Additional connection parameters.
        """
        self.db_backend = db_backend
        self.engine = None
        self.connection = None
        self.metadata = MetaData()

        if db_backend == "sqlite3":
            self._init_sqlite(**kwargs)
        elif db_backend == "mysql":
            self._init_mysql(**kwargs)
        elif db_backend == "postgresql":
            self._init_postgresql(**kwargs)
        else:
            logger.error("Unsupported database backend: %s", db_backend)
            sys.exit(1)

    def _init_sqlite(self, **kwargs):
        """Initialize SQLite database."""
        db_path = os.path.join(
            kwargs.get("ras_state_dir", ""), kwargs.get("ras_sqlite3_database", "")
        )
        self.engine = create_engine(f"sqlite:///{db_path}", echo=False)
        self.engine.connect()

    def _init_mysql(self, **kwargs):
        """Initialize MySQL/MariaDB database."""
        host = kwargs.get("mysql_conn_parms", {}).get("host", "localhost")
        port = kwargs.get("mysql_conn_parms", {}).get("port", "3306")
        user = kwargs.get("mysql_conn_parms", {}).get("user", "rasdaemon")
        password = kwargs.get("mysql_conn_parms", {}).get("password", "")
        database = kwargs.get("mysql_conn_parms", {}).get("database", "rasdaemon")
        socket = kwargs.get("mysql_conn_parms", {}).get("socket", "")
        use_ssl = kwargs.get("mysql_conn_parms", {}).get("use_ssl", "false")

        url = f"mysql+mysqlclient://{user}:{password}@{host}:{port}/{database}"
        if socket:
            url = f"mysql+mysqlclient://{user}:{password}@{socket}/{database}"
        self.engine = create_engine(url, echo=False)
        self.engine.connect()

    def _init_postgresql(self, **kwargs):
        """Initialize PostgreSQL database."""
        host = kwargs.get("postgresql_conn_parms", {}).get("host", "localhost")
        port = kwargs.get("postgresql_conn_parms", {}).get("port", "5432")
        user = kwargs.get("postgresql_conn_parms", {}).get("user", "rasdaemon")
        password = kwargs.get("postgresql_conn_parms", {}).get("password", "")
        database = kwargs.get("postgresql_conn_parms", {}).get("database", "rasdaemon")
        schema = kwargs.get("postgresql_conn_parms", {}).get("schema", "rasdaemon")
        ssl_mode = kwargs.get("postgresql_conn_parms", {}).get("ssl_mode", "false")
        connect_timeout = kwargs.get("postgresql_conn_parms", {}).get(
            "connect_timeout", "10"
        )

        ssl = f"sslmode={ssl_mode}" if ssl_mode != "false" else ""
        url = f"postgresql://{user}:{password}@{host}:{port}/{database}?{ssl}"
        self.engine = create_engine(url, echo=False)
        self.engine.connect()

    def execute(self, query: str, params: tuple = None) -> Optional[Any]:
        """
        Execute a SQL query and return the result.

        Args:
            query: SQL query string.
            params: Query parameters (optional).

        Returns:
            Query result or None.
        """

        try:
            with self.engine.connect() as conn:
                result = conn.execute(text(query), params or {})
                return result.fetchall()
        except OperationalError as e:
            logger.error("Database operation failed: %s", e)
            return None
        except ProgrammingError as e:
            logger.error("Programming error: %s", e)
            return None

    def execute_raw(self, query: str, params: tuple = None) -> Optional[Any]:
        """
        Execute a raw SQL query without using SQLAlchemy ORM.

        Args:
            query: SQL query string.
            params: Query parameters (optional).

        Returns:
            Query result or None.
        """

        try:
            with self.engine.connect() as conn:
                result = conn.execute(text(query), params or {})
                return result.fetchall()
        except OperationalError as e:
            logger.error("Database operation failed: %s", e)
            return None
        except ProgrammingError as e:
            logger.error("Programming error: %s", e)
            return None

    def create_index_if_not_exists(
        self, table_name: str, index_name: str, columns: List[str]
    ) -> bool:
        """
        Create an index if it doesn't exist.

        Args:
            table_name: Table name.
            index_name: Index name.
            columns: List of column names for the index.

        Returns:
            True if index was created, False otherwise.
        """

        try:
            with self.engine.connect() as conn:
                # Check if index exists
                check_query = f"SELECT name FROM sqlite_master WHERE type='index' AND name='{index_name}'"
                if self.db_backend == "sqlite3":
                    check_query = f"SELECT name FROM pg_indexes WHERE schemaname = 'public' AND indexname = '{index_name}'"

                result = conn.execute(text(check_query))
                if result.fetchall():
                    logger.info(
                        "Index '%s' already exists on table '%s'",
                        index_name,
                        table_name,
                    )
                    return False
                else:
                    # Create index
                    if self.db_backend == "sqlite3":
                        create_query = f"CREATE INDEX {index_name} ON {table_name} ({', '.join(columns)})"
                    else:
                        create_query = f"CREATE INDEX {index_name} ON {table_name} ({', '.join(columns)})"

                    conn.execute(text(create_query))
                    logger.info(
                        "Created index '%s' on table '%s'", index_name, table_name
                    )
                    return True
        except Exception as e:
            logger.error("Failed to create index: %s", e)
            return False

    def close(self):
        """Close the database connection."""
        if self.engine:
            self.engine.dispose()
