RAS Daemon
==========

rasdaemon monitors Linux kernel trace events for Reliability, Availability and
Serviceability (RAS) errors. It reports decoded events through syslog or
journald and can record them in SQLite, MySQL/MariaDB, or PostgreSQL.

Its documentation is available at:

* https://mchehab.github.io/rasdaemon/

Building
--------

rasdaemon requires Meson 0.60 or newer, Ninja, a C compiler, libtraceevent,
Python, and libpci. Database backends and unit tests have additional optional
dependencies described in the `building and installation guide
<https://mchehab.github.io/rasdaemon/installation.html>`_.

Configure and build with::

   $ meson setup build
   $ meson compile -C build

The top-level Makefile provides a convenience wrapper for the default build::

   $ make

Install from the configured build directory with::

   # meson install -C build

Running
-------

rasdaemon normally requires root permission to access kernel tracing nodes.
Run it in the foreground with::

   # rasdaemon --foreground

To record events using the selected database backend, add ``--record``::

   # rasdaemon --foreground --record

The backend defaults to SQLite. See the `database guide
<https://mchehab.github.io/rasdaemon/databases.html>`_ for configuring SQLite,
MySQL/MariaDB, or PostgreSQL. Use ``rasdaemon --help`` to list the options
compiled into the installed binary.

Documentation and development
-----------------------------

The documentation includes:

* `Installation and build options
  <https://mchehab.github.io/rasdaemon/installation.html>`_
* `Running rasdaemon
  <https://mchehab.github.io/rasdaemon/running.html>`_
* `Runtime configuration
  <https://mchehab.github.io/rasdaemon/configuration.html>`_
* `Testing <https://mchehab.github.io/rasdaemon/testing.html>`_
* `Developer guide
  <https://mchehab.github.io/rasdaemon/development.html>`_
* `API reference <https://mchehab.github.io/rasdaemon/api.html>`_
* `Contributing
  <https://mchehab.github.io/rasdaemon/contributing.html>`_

The primary source repository and issue tracker are at
https://github.com/mchehab/rasdaemon/.
