RAS Daemon
==========

Those tools provide a way to get Platform Reliability, Availability
and Serviceability (RAS) reports made via the Kernel tracing events.

The main repository for the rasdaemon is at:

- <https://github.com/mchehab/rasdaemon>

And two mirrors are available:

- <https://gitlab.com/mchehab_kernel/rasdaemon>
- <http://git.infradead.org/users/mchehab/rasdaemon.git>

Tarballs for each release can be found at:
- <http://www.infradead.org/~mchehab/rasdaemon/>

Intended Use
============

This project provides general-purpose software components intended primarily
for integration, development, research, and infrastructure use by technical
users.

The project is not offered as a consumer-facing online service or
managed platform.

Goals
=====

Its initial goal is to replace the edac-tools that got bitrotted after
the addition of the HERM (Hardware Events Report Method )patches[^1] at
the EDAC Kernel drivers.

[^1]: <http://lkml.indiana.edu/hypermail/linux/kernel/1205.1/02075.html>

Its long term goal is to be the userspace tool that will collect all
hardware error events reported by the Linux Kernel from several sources
(EDAC, MCE, PCI, ...) into one common framework.

It is not meant to provide tools for doing error injection, as there are
other tools already covering it, like:
<git://git.kernel.org/pub/scm/linux/kernel/git/gong.chen/mce-test.git>

Yet, a few set of testing scripts are provided under /contrib dir.

When the final version of the HERM patches was merged upstream, it was
decided to not expose the memory error counters to userspace.
This is one of the differences from what it was provided by edac-utils, as
EDAC 2.0.0 exports errors via a set of sysfs nodes that sums the amount of
errors per DIMM, per memory channel and per memory controller.
However, those counters are monotonically increased, and there's no way to
detect if they're very sparse in time, if the occurrence is increasing over
time, or if they're due to some burst, perhaps due to a Solar Storm hitting
the ionosphere.

In other words, the rationale for not exposing such the information is that:


1. can be easily accounted on userspace;
2. they're not really meaningful. E. g. one system with, let's say
   10 corrected errors can be fine, while another one with the same amount
   of errors can have problems, as the error counters don't take into
   account things like system uptime, memory error bursts (that could be
   caused by a solar storm, for example), etc.

So, the idea since them was to make the kernel-userspace interface
simpler and move the policy to the userspace daemon. It is up to the
userspace daemon to correlate the data about the RAS events and provide
the system administrator a comprehensive report, presenting him
a better hint if he needs to contact the hardware vendor to replace
a component that is working degraded, or to simply discard the error.

So, the approach taken here is to allow storing those errors on a SQLite
database, in order to allow those data to be latter mining.

It is currently not part of the scope to do sophiscicated data mining
analysis, as that would require enough statistitical data about hardware
MTBF. In other words, an abnormal component that needs to be replaced
shoud be statistically compared with a similar component that operates
under a normal condition. To do such checks, the analysis tool would
need to know the probability density function(p. d. f.) of that component,
and its relevant parameters (like mean and standard derivation, if the
p. d. f. funcion is a Normal distribution).

While this tool works since Kernel 3.5 (where HERM patches got added),
in order to get the full benefit of this tool, Kernel 3.10 or upper is
needed.

Compiling and Installing
========================

Meson, a C compiler, the trace-event library, and the Python dependencies
need to be installed. On Fedora, this is done by installing the following
packages:

```
    gcc
    meson
    ninja-build
    libtraceevent-devel
    pciutils-devel
    python3
    python3-sqlalchemy
    sqlite-devel                (if SQLite3 will be used)
    mariadb-connector-c-devel   (if MariaDB will be used)
    mysql-community-devel       (if Oracle MySQL will be used; from the MySQL repository)
    libpq-devel                 (if PostgreSQL will be used)
    python3-mysqlclient         (to query either MariaDB or MySQL with ras-mc-ctl)
    python3-psycopg2            (to query PostgreSQL with ras-mc-ctl)
```

To install them on Fedora, run:
```
    $ dnf install -y gcc meson ninja-build libtraceevent-devel \
      pciutils-devel python3 python3-sqlalchemy sqlite-devel
```

All features are enabled by default when their dependencies are available.
Features can be explicitly enabled or disabled with Meson options. For
example:

```
    -Dsqlite3=enabled    enable storage in an SQLite3 database
    -Daer=enabled        enable PCIe AER events
    -Dmce=enabled        enable MCE events
```

In order to compile it, run:
```
    $ meson setup build [options]
    $ meson compile -C build
```

The top-level Makefile is a convenience wrapper around Meson and Ninja. For a
build using the default options, simply running the following also works:

```
    $ make
```

So, for example, to enable everything but sqlite3:

```
    $ meson setup build -Dsqlite3=disabled
    $ meson compile -C build
```

After compiling, run, as root:
```
    $ meson install -C build
```

When using the Makefile wrapper, `make install` performs the equivalent
installation.

RPM-based compilation
=====================

If the distribution is rpm-based, an alternative method would be to do:
```
    $ meson setup build
```

The above procedure will generate a file at misc/rasdaemon.spec.

You may edit it to change the Meson feature options.

To generate the rpm files, do:

```
    # make mock
```

To install the rpm files, run, as root:
```
    # rpm -i $(ls SRPMS/rasdaemon-*.rpm|tail -1)
```

Running
=======

The daemon generally requires root permission, in order to read the
needed debugfs trace nodes, with needs to be previously mounted.
The rasdaemon will check at /proc/mounts where the debugfs partition
is mounted and use it while running.

To run the rasdaemon in background, just call it without any parameters:

```
    # rasdaemon
```

The output will be available via syslog. Or, to run it in foreground and see
the logs in console, run it as:

```
    # rasdaemon -f
```

or, if you also want to record errors at the database (--enable-sqlite3 is
required):

```
    # rasdaemon -f -r
```

To post-process and decode received MCA errors on AMD SMCA systems, run:

```
	# rasdaemon -p --status <STATUS_reg> --ipid <IPID_reg> --smca --family <CPU Family> --model <CPU Model> --bank <BANK_NUM>
```

Status and IPID Register values (in hex) are mandatory. The `smca` flag
with `family` and `model` are required if not decoding locally. `Bank`
parameter is optional.

You may also start it via systemd:

```
    # systemctl start rasdaemon
```

The rasdaemon will then output the messages to journald.

How to Setup a Database
=======================

RAS Daemon supports multiple types of databases. Each require their own
specific parameters. Only one database can be active. The database backend
connection parameters are specified via environment variables, usually in
the `/etc/sysconfig/rasdaemon` file. It contains these common fields:

- `RASDAEMON_DB_BACKEND` - backend to be used. Can be: `postgresql`,
  `mysql` or `sqlite3`.
- `RASDAEMON_HOSTNAME` - hostname to be stored inside remote databases
   (MySQL/MariaDB or PostgreSQL). Not used for SQLite.

The other parameters are database specific and are also located in that file.

## SQLite3 (default)

For SQLite3, `RAS_SQLITE3_DATABASE` specifies the full database path. It
defaults to `/var/lib/rasdaemon/ras-mc_event.db`, and rasdaemon creates the
parent directory when needed. The compiled default can be changed with:

```
    $ meson setup build -Dsqlite3-database=/path/to/ras-mc_event.db
```

On Ubuntu/Debian, the packages required to build rasdaemon with sqlite3
support are:

```bash
sudo apt-get update
sudo apt-get install -y build-essential meson ninja-build pkg-config \
    git cmake libsqlite3-dev sqlite3 \
    libtraceevent-dev libtraceevent1 libpci-dev
```

## PostgreSQL

To use PostgreSQL, set the following environment variables
(all optional with sensible defaults):

- `RAS_PG_HOST` - Hostname or IP (default: `""` = local Unix socket)
- `RAS_PG_PORT` - TCP port (default: `5432`)
- `RAS_PG_USER` - Username (default: `rasdaemon`)
- `RAS_PG_PASSWORD` - Password (default: empty)
- `RAS_PG_SCHEMA` - Schema (default: `rasdaemon`)
- `RAS_PG_DATABASE` - Database name (default: `rasdaemon`)
- `RAS_PG_SSL_MODE` - Optional libpq SSL mode (default: `prefer`)
- `RAS_PG_CONNECT_TIMEOUT` - Connection timeout in seconds (default: `10`)

Example:
```bash
RAS_PG_HOST=""
RAS_PG_PORT="5432"
RAS_PG_USER="rasdaemon"
RAS_PG_PASSWORD=""
RAS_PG_DATABASE="rasdaemon"
RAS_PG_SCHEMA="rasdaemon"
RAS_PG_SSL_MODE="prefer"
RAS_PG_CONNECT_TIMEOUT="10"
```

**Prerequisites**: Install `libpq-devel` or `libpq-dev` package and
`postgresql` server.

### Install and configure PostgreSQL

On Debian/Ubuntu:

```bash
sudo apt-get install -y postgresql postgresql-client libpq-dev

sudo systemctl start postgresql
for attempt in {1..30}; do
  if pg_isready --host=127.0.0.1 --port=5432; then
    exit 0
  fi
  sleep 1
done

sudo systemctl status postgresql --no-pager
```

### Setup PostgreSQL trust authentication

If you plan to run PostgreSQL locally starting it from a different user,
you'll need to setup the the rasdaemon user as trusted.

```bash
HBA_FILE="$(sudo -u postgres psql --tuples-only --no-align --command='SHOW hba_file')"

sudo sed -i '1i local all rasdaemon trust' "$HBA_FILE"

echo "Postgres config file $HBA_FILE:"
sudo cat "$HBA_FILE"

sudo systemctl restart postgresql
for attempt in {1..30}; do
  if pg_isready --host=127.0.0.1 --port=5432; then
    exit 0
  fi
  sleep 1
done

# Check if everything is OK
sudo systemctl status postgresql --no-pager
```

### Create PostgreSQL database and schema

Before starting rasdaemon, if they don't exist, you need to:
- create the user;
- create the database;
- create the schema;
- grant permissions.

For a simple setup, you could do:

```bash
sudo -u postgres psql --set ON_ERROR_STOP=1 << EOF
  CREATE USER rasdaemon WITH PASSWORD 'some_password';
  CREATE DATABASE rasdaemon OWNER rasdaemon;
EOF

PGPASSWORD='some_password' \
  psql -h /var/run/postgresql -U rasdaemon \
       -d rasdaemon --set ON_ERROR_STOP=1 << EOF2
  CREATE SCHEMA rasdaemon AUTHORIZATION rasdaemon;
EOF2
```

## MySQL / MariaDB

To use MySQL or MariaDB, set the following environment variables
(all optional with sensible defaults):

- `RAS_MYSQL_HOST` - Hostname or IP (default: `""` = local Unix socket)
- `RAS_MYSQL_PORT` - TCP port (default: `3306`)
- `RAS_MYSQL_USER` - Username (default: `rasdaemon`)
- `RAS_MYSQL_PASSWORD` - Password (default: empty)
- `RAS_MYSQL_DATABASE` - Database name (default: `rasdaemon_test`)

Example:
```bash
RAS_MYSQL_HOST=""
RAS_MYSQL_PORT="3306"
RAS_MYSQL_USER="rasdaemon"
RAS_MYSQL_PASSWORD=""
RAS_MYSQL_DATABASE="rasdaemon_test"
```

The C client development package depends on the server implementation and
distribution:

- Fedora/RHEL with MariaDB: `mariadb-connector-c-devel`
- Fedora/RHEL with Oracle MySQL packages: `mysql-community-devel`
- Debian/Ubuntu with MariaDB: `libmariadb-dev`; some releases also require
  `libmariadb-dev-compat` for the MySQL-compatible development headers
- Debian/Ubuntu with MySQL: `default-libmysqlclient-dev`

Only one C client development package is needed. Meson accepts the
`mysqlclient`, `mariadb`, or `libmariadb` pkg-config interface. The Python
database tool additionally requires the `mysqlclient` Python module, packaged
as `python3-mysqlclient` on Fedora/RHEL and `python3-mysqldb` on Debian/Ubuntu.

### Start MySQL or MariaDB and create the database

For MySQL and MariaDB, you'll need to:
- create the user;
- create the database;
- grant permissions.

The service name is normally `mysqld.service` for Oracle MySQL packages and
`mariadb.service` for MariaDB. On Debian/Ubuntu, the MySQL service may instead
be named `mysql.service`. Start the service matching the installed server, for
example:

```bash
sudo systemctl start mariadb.service
# Or: sudo systemctl start mysqld.service
# Or: sudo systemctl start mysql.service
```

For a simple local setup, the SQL is the same for both implementations:

```bash
mysqladmin ping --host=127.0.0.1 --user=root --password=root

sudo -u root mysql --host=127.0.0.1 << EOF
  CREATE USER 'rasdaemon'@'%' IDENTIFIED BY 'mypass';
  CREATE DATABASE  rasdaemon_test;
  GRANT ALL PRIVILEGES ON rasdaemon_test.* TO 'rasdaemon'@'%';
  FLUSH PRIVILEGES;
EOF
```

Unit Tests
==========

rasdaemon provides unit tests for each database backend. The tests follow
the pattern shown in `tests/db*` [6, 9, 4], which use `module_init()` to
initialize exactly one backend module.

To enable unit tests, `libcmocka-devel` or `libcmocka-dev` package is
required.

**SQLite3 tests** (`test-sqlite3.c` [6]):
```bash
$ make
$ ./build/unittest -g sqlite3
```

**PostgreSQL tests** (`test-postgresql.c` [4, 5]):
```bash
$ make
$ ./build/unittest -g postgresql
```

**MySQL tests** (`test-mysql.c` [9]):
```bash
$ make
$ ./build/unittest -g mysql
```

Error Injection Testing
=======================

Some scripts is provided under /contrib, in order to test the daemon EDAC
handler you can use:

- `contrib/edac-fake-inject` - Inject EDAC errors.
  Requires a kernel compiled with `CONFIG_EDAC_DEBUG` and a
  running EDAC driver.

For other error sources, external tools are recommended:

- **MCE error injection**: <https://git.kernel.org/pub/scm/utils/cpu/mce/mce-inject.git>
- **APEI error injection**: <https://git.kernel.org/pub/scm/linux/kernel/git/gong.chen/mce-test.git/>
- **AER error injection**: <https://git.kernel.org/pub/scm/linux/kernel/git/gong.chen/aer-inject.git/>



A script is provided under /contrib, in order to test the daemon EDAC
handler. While the daemon is running, just run:

```
# contrib/edac-fake-inject
```

Submitting Patches
==================

If you want to help improve this tool, be my guest! We try to follow
the Kernel's CodingStyle and submission rules as a reference.

Before submitting your patch, please check the coding style with:
scripts/checkpatch.pl.

In order to contribute with rasdaemon, please send a Merge Request via
github repository at:

- <https://github.com/mchehab/rasdaemon>

Or, alternatively, send a pull request against gitlab repository at:

- <https://gitlab.com/mchehab_kernel/rasdaemon>

It is also recommended to send patches to <linux-edac@vger.kernel.org>
with a copy to:

- Mauro Carvalho Chehab \<<mchehab@kernel.org>\>

Please notice that github is the preferred way. If you're not using
it, please be kind enough to add an issue there for us to track the
patch series.

Don't forget to add a description of the patch in the body of the email, adding
a Signed-off-by: at the end of the patch description (before the unified diff
with the patch).

We use Signed-off-by the same way as in kernel, so I'm transcribing
bellow the same text as found under Kernel's Documentation/SubmittingPatches:

```
   "To improve tracking of who did what, especially with patches that can
    percolate to their final resting place in the kernel through several
    layers of maintainers, we've introduced a "sign-off" procedure on
    patches that are being emailed around.

    The sign-off is a simple line at the end of the explanation for the
    patch, which certifies that you wrote it or otherwise have the right to
    pass it on as an open-source patch.  The rules are pretty simple: if you
    can certify the below:

	    Developer's Certificate of Origin 1.1

	    By making a contribution to this project, I certify that:

	    (a) The contribution was created in whole or in part by me and I
		have the right to submit it under the open source license
		indicated in the file; or

	    (b) The contribution is based upon previous work that, to the best
		of my knowledge, is covered under an appropriate open source
		license and I have the right under that license to submit that
		work with modifications, whether created in whole or in part
		by me, under the same open source license (unless I am
		permitted to submit under a different license), as indicated
		in the file; or

	    (c) The contribution was provided directly to me by some other
		person who certified (a), (b) or (c) and I have not modified
		it.

	    (d) I understand and agree that this project and the contribution
		are public and that a record of the contribution (including all
		personal information I submit with it, including my sign-off) is
		maintained indefinitely and may be redistributed consistent with
		this project or the open source license(s) involved.

    then you just add a line saying

	    Signed-off-by: Random J Developer <random@developer.example.org>

    using your real name (sorry, no pseudonyms or anonymous contributions.)"

```
