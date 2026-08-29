.. SPDX-License-Identifier: GPL-2.0-only

Database Backends
=================

rasdaemon supports SQLite, MySQL/MariaDB, and PostgreSQL. Multiple backends
may be compiled in, but only one can be active in a daemon process. Connection
parameters are environment variables, normally set in the installed
``sysconfig/rasdaemon`` file.

The common settings are:

``RASDAEMON_DB_BACKEND``
   Select ``sqlite3``, ``mysql``, or ``postgresql``. The default is ``sqlite3``.

``RASDAEMON_HOSTNAME``
   Override the hostname stored by the MySQL/MariaDB or PostgreSQL backend.
   This is not used by SQLite.

SQLite
------

``RAS_SQLITE3_DATABASE`` specifies the full database path. It defaults to
``/var/lib/rasdaemon/ras-mc_event.db``; rasdaemon creates the parent directory
when needed. Change the compiled default with::

   $ meson setup build \
       -Dsqlite3-database=/path/to/ras-mc_event.db

On Ubuntu or Debian, install the SQLite build dependencies with::

   $ sudo apt-get update
   $ sudo apt-get install -y build-essential meson ninja-build pkg-config \
       git cmake libsqlite3-dev sqlite3 libtraceevent-dev libtraceevent1 \
       libpci-dev

PostgreSQL
----------

The PostgreSQL backend accepts:

``RAS_PG_HOST``
   Hostname or address. The default is an empty string, which selects a local
   Unix socket.

``RAS_PG_PORT``
   TCP port. The default is ``5432``.

``RAS_PG_USER``
   User name. The default is ``rasdaemon``.

``RAS_PG_PASSWORD``
   Password. The default is empty.

``RAS_PG_SCHEMA``
   Schema. The default is ``rasdaemon``.

``RAS_PG_DATABASE``
   Database name. The default is ``rasdaemon``.

``RAS_PG_SSL_MODE``
   Optional libpq SSL mode. The libpq default is ``prefer``.

``RAS_PG_USE_SSL``
   Require TLS when no explicit SSL mode is supplied. The value is ``true`` or
   ``false`` and defaults to ``false``.

``RAS_PG_CONNECT_TIMEOUT``
   Connection timeout in seconds. The default is ``10``.

For example::

   RAS_PG_HOST=""
   RAS_PG_PORT="5432"
   RAS_PG_USER="rasdaemon"
   RAS_PG_PASSWORD=""
   RAS_PG_DATABASE="rasdaemon"
   RAS_PG_SCHEMA="rasdaemon"
   RAS_PG_SSL_MODE="prefer"
   RAS_PG_USE_SSL="false"
   RAS_PG_CONNECT_TIMEOUT="10"

Install ``libpq-devel`` or ``libpq-dev`` to build this backend. On Debian or
Ubuntu, install and start PostgreSQL with::

   $ sudo apt-get install -y postgresql postgresql-client libpq-dev
   $ sudo systemctl start postgresql
   $ pg_isready --host=127.0.0.1 --port=5432

For a simple local setup, create a role, database, and schema before starting
rasdaemon::

   $ sudo -u postgres psql --set ON_ERROR_STOP=1 << EOF
     CREATE USER rasdaemon WITH PASSWORD 'some_password';
     CREATE DATABASE rasdaemon OWNER rasdaemon;
   EOF

   $ PGPASSWORD='some_password' \
       psql -h /var/run/postgresql -U rasdaemon -d rasdaemon \
       --set ON_ERROR_STOP=1 << EOF
     CREATE SCHEMA rasdaemon AUTHORIZATION rasdaemon;
   EOF

If local authentication does not allow the ``rasdaemon`` role to connect,
configure the appropriate rule in ``pg_hba.conf`` and restart PostgreSQL.

MySQL and MariaDB
-----------------

The MySQL/MariaDB backend accepts:

``RAS_MYSQL_HOST``
   Hostname or address. The default is an empty string, which selects a local
   Unix socket.

``RAS_MYSQL_PORT``
   TCP port. The default is ``3306``.

``RAS_MYSQL_USER``
   User name. The default is ``rasdaemon``.

``RAS_MYSQL_PASSWORD``
   Password. The default is empty.

``RAS_MYSQL_DATABASE``
   Database name. The default is ``rasdaemon``.

``RAS_MYSQL_SOCKET``
   Optional Unix socket path for local connections.

``RAS_MYSQL_USE_SSL``
   Require TLS. The value is ``true`` or ``false`` and defaults to ``false``.

``RAS_MYSQL_CONNECT_TIMEOUT``
   Connection timeout in seconds. The default is ``10``.

For example::

   RAS_MYSQL_HOST=""
   RAS_MYSQL_PORT="3306"
   RAS_MYSQL_USER="rasdaemon"
   RAS_MYSQL_PASSWORD=""
   RAS_MYSQL_DATABASE="rasdaemon"
   RAS_MYSQL_SOCKET=""
   RAS_MYSQL_USE_SSL="false"
   RAS_MYSQL_CONNECT_TIMEOUT="10"

The required C development package depends on the implementation and
distribution:

* Fedora/RHEL with MariaDB: ``mariadb-connector-c-devel``
* Fedora/RHEL with Oracle MySQL: ``mysql-community-devel``
* Debian/Ubuntu with MariaDB: ``libmariadb-dev`` and, on some releases,
  ``libmariadb-dev-compat``
* Debian/Ubuntu with MySQL: ``default-libmysqlclient-dev``

Only one C client development package is needed. Meson recognizes the
``mysqlclient``, ``mariadb``, and ``libmariadb`` pkg-config interfaces. The
``ras-mc-ctl`` Python tool also requires ``python3-mysqlclient`` on Fedora/RHEL
or ``python3-mysqldb`` on Debian/Ubuntu.

Start the server using the service name supplied by the distribution, commonly
``mariadb.service``, ``mysqld.service``, or ``mysql.service``. A simple setup
can be created with::

   $ sudo mysql << EOF
     CREATE USER 'rasdaemon'@'%' IDENTIFIED BY 'some_password';
     CREATE DATABASE rasdaemon;
     GRANT ALL PRIVILEGES ON rasdaemon.* TO 'rasdaemon'@'%';
     FLUSH PRIVILEGES;
   EOF
