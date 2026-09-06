RAS Daemon
==========

rasdaemon monitors Linux kernel trace events for Reliability, Availability and
Serviceability (RAS) errors. It reports decoded events through syslog or
journald and can record them in SQLite, MySQL/MariaDB, or PostgreSQL.

The primary source repository and issue tracker is https://github.com/mchehab/rasdaemon/.

External CI reports
-------------------

.. |func-kernel-pass| image:: https://mchehab.github.io/rasdaemon-ci/daily/badge-kernel-pass.svg
   :target: https://mchehab.github.io/rasdaemon-ci/daily/
.. |func-kernel-fail| image:: https://mchehab.github.io/rasdaemon-ci/daily/badge-kernel-fail.svg
   :target: https://mchehab.github.io/rasdaemon-ci/daily/
.. |func-kernel-skip| image:: https://mchehab.github.io/rasdaemon-ci/daily/badge-kernel-skip.svg
   :target: https://mchehab.github.io/rasdaemon-ci/daily/
.. |func-kernel-n-a| image:: https://mchehab.github.io/rasdaemon-ci/daily/badge-kernel-n-a.svg
   :target: https://mchehab.github.io/rasdaemon-ci/daily/
.. |func-rasdaemon-pass| image:: https://mchehab.github.io/rasdaemon-ci/daily/badge-rasdaemon-pass.svg
   :target: https://mchehab.github.io/rasdaemon-ci/daily/
.. |func-rasdaemon-fail| image:: https://mchehab.github.io/rasdaemon-ci/daily/badge-rasdaemon-fail.svg
   :target: https://mchehab.github.io/rasdaemon-ci/daily/
.. |func-rasdaemon-skip| image:: https://mchehab.github.io/rasdaemon-ci/daily/badge-rasdaemon-skip.svg
   :target: https://mchehab.github.io/rasdaemon-ci/daily/
.. |func-rasdaemon-n-a| image:: https://mchehab.github.io/rasdaemon-ci/daily/badge-rasdaemon-n-a.svg
   :target: https://mchehab.github.io/rasdaemon-ci/daily/

The separate `rasdaemon-ci project <https://github.com/mchehab/rasdaemon-ci>`_
publishes system-level test results independently of this repository.

The `latest daily functional-test report
<https://mchehab.github.io/rasdaemon-ci/daily/>`_ summary can be seen here:

+------------------+----------------+-----------------------+-----------------------+-----------------------+-----------------------+
| Test type        | Scope          | PASS                  | FAIL                  | SKIP                  | N/A                   |
+==================+================+=======================+=======================+=======================+=======================+
| Functional tests | Kernel         | |func-kernel-pass|    | |func-kernel-fail|    | |func-kernel-skip|    | |func-kernel-n-a|     |
|                  +----------------+-----------------------+-----------------------+-----------------------+-----------------------+
|                  | RAS Daemon     | |func-rasdaemon-pass| | |func-rasdaemon-fail| | |func-rasdaemon-skip| | |func-rasdaemon-n-a|  |
+------------------+----------------+-----------------------+-----------------------+------------------------+----------------------+

Building
--------

rasdaemon requires Meson 0.60 or newer, Ninja, a C compiler, libtraceevent,
Python, and libpci. Database backends and unit tests have additional optional
dependencies described in the `building and installation guide
<https://mchehab.github.io/rasdaemon/installation.html>`_.

Configure and build with::

   $ make

Install from the configured build directory with::

   $ sudo make install

Running
-------

rasdaemon requires root permission to access Linux Kernel tracing nodes and
other error events.

Run it in the foreground with ``--foreground`` or ``-f``::

   $ sudo rasdaemon -f

To record events on a database (by default SQLite3), add add ``--record`` or ``-r``::

   $ sudo rasdaemon -f -r

Checking errors
---------------

To see a summary of rasdaemon found errors (when ``--record`` is used)::

   $ sudo ras-mc-ctl db --errors

The backend defaults to SQLite. See the `database guide
<https://mchehab.github.io/rasdaemon/databases.html>`_ for configuring SQLite,
MySQL/MariaDB, or PostgreSQL. Use ``rasdaemon --help`` to list the options
compiled into the installed binary.

Documentation
-------------

See https://mchehab.github.io/rasdaemon/

User's documentation
~~~~~~~~~~~~~~~~~~~~

* `Installation and build options <https://mchehab.github.io/rasdaemon/installation.html>`_
* `Running rasdaemon <https://mchehab.github.io/rasdaemon/running.html>`_
* `Using ras-mc-ctl <https://mchehab.github.io/rasdaemon/ras-mc-ctl.html>`_
* `Runtime configuration <https://mchehab.github.io/rasdaemon/configuration.html>`_
* `Testing <https://mchehab.github.io/rasdaemon/testing.html>`_

Developer's documentation
~~~~~~~~~~~~~~~~~~~~~~~~~

* `Developer guide <https://mchehab.github.io/rasdaemon/development.html>`_
* `API reference <https://mchehab.github.io/rasdaemon/api.html>`_
* `Contributing <https://mchehab.github.io/rasdaemon/contributing.html>`_

Security patches
~~~~~~~~~~~~~~~~

* `Security and Responsible Disclosure <https://mchehab.github.io/rasdaemon/security.html>`_
