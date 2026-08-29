.. SPDX-License-Identifier: GPL-2.0-only

Building and Installing
=======================

rasdaemon requires Meson 0.60 or newer, Ninja, a C compiler, libtraceevent,
and Python. Sphinx is optional and enables the HTML documentation target;
cmocka is optional and enables unit tests.

On Fedora, the relevant packages are::

   gcc
   meson
   ninja-build
   libtraceevent-devel
   pciutils-devel
   python3
   python3-sqlalchemy
   python3-sphinx              (to build this documentation)
   libcmocka-devel             (to build unit tests)
   sqlite-devel               (if SQLite will be used)
   mariadb-connector-c-devel  (if MariaDB will be used)
   mysql-community-devel      (if Oracle MySQL will be used)
   libpq-devel                (if PostgreSQL will be used)
   python3-mysqlclient        (to query MySQL/MariaDB with ras-mc-ctl)
   python3-psycopg2           (to query PostgreSQL with ras-mc-ctl)

For example, install the basic Fedora build dependencies with::

   $ dnf install -y gcc meson ninja-build libtraceevent-devel \
       pciutils-devel python3 python3-sqlalchemy sqlite-devel

Configure and build
-------------------

All features are enabled by default when their dependencies are available.
Features can be explicitly enabled or disabled with Meson options. For
example::

   -Dsqlite3=enabled  enable storage in an SQLite database
   -Daer=enabled      enable PCIe AER events
   -Dmce=enabled      enable MCE events

Use ``meson configure build`` after setup to see every option and its current
value. ``-Denable-arch=auto`` selects the build-host architecture; ``x86``,
``arm``, and ``riscv`` select one architecture family, while ``all`` is useful
for cross-architecture testing. ``-Ddisable-all=true`` provides a minimal
build for dependency and feature-isolation checks.

Configure and compile with::

   $ meson setup build [options]
   $ meson compile -C build

The top-level Makefile is a convenience wrapper around Meson and Ninja. For a
build using the default options, this is sufficient::

   $ make

For example, to disable SQLite::

   $ meson setup build -Dsqlite3=disabled
   $ meson compile -C build

Install as root with::

   # meson install -C build

When using the Makefile wrapper, ``make install`` performs the equivalent
installation.

RPM packages
------------

On RPM-based distributions, configure the project first::

   $ meson setup build

This generates ``misc/rasdaemon.spec``. You may edit the generated file to
change Meson feature options. Build RPMs with::

   # make mock

Install the resulting package with::

   # rpm -i $(ls SRPMS/rasdaemon-*.rpm | tail -1)
