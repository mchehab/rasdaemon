RAS Daemon
==========

RAS Daemon (rasdaemon) monitors Linux kernel trace events related to
Reliability, Availability, and Serviceability (RAS) errors.

Although it is a general-purpose tool, its primary goal is to improve
the availability of systems used in data centers, including - but not limited
to - high-performance computing (HPC), server farms, and cloud service
environments.

It reports decoded events through syslog or journald, makes them
available to consumer applications and supports recording them in SQL
databases and reporting them through ABRT.

Its database support is provided through a modular framework that currently
supports SQLite, MySQL, MariaDB, and PostgreSQL.

The primary source repository and issue tracker is currently at
https://github.com/mchehab/rasdaemon/.

Its source code is mirrored at:

- https://gitlab.com/mchehab_kernel/rasdaemon
- http://git.infradead.org/users/mchehab/rasdaemon.git

CI tests
--------

.. |unit-rasdaemon-pass| image:: https://mchehab.github.io/rasdaemon/test-badges/rasdaemon-pass.svg
   :target: https://mchehab.github.io/rasdaemon/test-badges/rasdaemon.html
.. |unit-rasdaemon-fail| image:: https://mchehab.github.io/rasdaemon/test-badges/rasdaemon-fail.svg
   :target: https://mchehab.github.io/rasdaemon/test-badges/rasdaemon.html
.. |unit-ras-mc-ctl-pass| image:: https://mchehab.github.io/rasdaemon/test-badges/ras-mc-ctl-pass.svg
   :target: https://mchehab.github.io/rasdaemon/test-badges/ras-mc-ctl.html
.. |unit-ras-mc-ctl-fail| image:: https://mchehab.github.io/rasdaemon/test-badges/ras-mc-ctl-fail.svg
   :target: https://mchehab.github.io/rasdaemon/test-badges/ras-mc-ctl.html
.. _Unit tests: https://mchehab.github.io/rasdaemon/test-badges/
.. |func-feature-pass| image:: https://mchehab.github.io/rasdaemon-ci/daily/badge-feature-pass.svg
   :target: https://mchehab.github.io/rasdaemon-ci/daily/
.. |func-feature-fail| image:: https://mchehab.github.io/rasdaemon-ci/daily/badge-feature-fail.svg
   :target: https://mchehab.github.io/rasdaemon-ci/daily/
.. |func-kernel-pass| image:: https://mchehab.github.io/rasdaemon-ci/daily/badge-kernel-pass.svg
   :target: https://mchehab.github.io/rasdaemon-ci/daily/
.. |func-kernel-fail| image:: https://mchehab.github.io/rasdaemon-ci/daily/badge-kernel-fail.svg
   :target: https://mchehab.github.io/rasdaemon-ci/daily/
.. |func-kernel-skip| image:: https://mchehab.github.io/rasdaemon-ci/daily/badge-kernel-skip.svg
   :target: https://mchehab.github.io/rasdaemon-ci/daily/
.. |func-rasdaemon-pass| image:: https://mchehab.github.io/rasdaemon-ci/daily/badge-rasdaemon-pass.svg
   :target: https://mchehab.github.io/rasdaemon-ci/daily/
.. |func-rasdaemon-fail| image:: https://mchehab.github.io/rasdaemon-ci/daily/badge-rasdaemon-fail.svg
   :target: https://mchehab.github.io/rasdaemon-ci/daily/
.. |func-rasdaemon-skip| image:: https://mchehab.github.io/rasdaemon-ci/daily/badge-rasdaemon-skip.svg
   :target: https://mchehab.github.io/rasdaemon-ci/daily/
.. |func-x86-vm| image:: https://mchehab.github.io/rasdaemon-ci/daily/badge-x86-vm-fail.svg
   :target: https://mchehab.github.io/rasdaemon-ci/daily/
.. |func-arm64-vm| image:: https://mchehab.github.io/rasdaemon-ci/daily/badge-arm64-vm-fail.svg
   :target: https://mchehab.github.io/rasdaemon-ci/daily/
.. _Functional tests: https://mchehab.github.io/rasdaemon-ci/daily/

RAS Daemon has two types of CI tests, automated via Github Actions:

1. `rasdaemon Actions <https://github.com/mchehab/rasdaemon/actions>`_, with
   tests several unit tests for both ``rasdaemon`` and ``ras-mc-ctl`` tools.

2. `rasdaemon-ci functional tests <https://github.com/mchehab/rasdaemon-ci>`_,
   which runs rasdaemon on a QEMU engine, using some mechanisms supported by
   QEMU to trigger events on it. The functional tests are handled in separate.
   CI tests usually runs on source code changes and are daily updated.

   Such tests are executed on two separate QEMU VMs. The status indicators
   below shows green if both VMs executed fine at the latest run:

   +---------------------+----------------+------------------------+
   | VM health           | Architecture   | Status                 |
   +=====================+================+========================+
   | Functional tests    | x86_64         | |func-x86-vm|          |
   | (experimental)      +----------------+------------------------+
   |                     | aarch64        | |func-arm64-vm|        |
   +---------------------+----------------+------------------------+

   **NOTE**:
      Please note that **functional tests** create virtual machines configured
      to receive non-fatal errors and report them to rasdaemon. These tests use
      the latest stable versions of the Linux kernel and QEMU.

      By their nature, failures in these tests may be false positives, which
      are far more common than regressions in QEMU, the Linux kernel, or
      rasdaemon. Before reporting a regression, check the rasdaemon-ci logs.
      If the problem is in the toolset, open an issue against the affected
      tool rather than rasdaemon.

The results from its latest run are automatically updated in this panel:

+---------------------+----------------+------------------------+------------------------+
| Test type           | Scope          | PASS                   | FAIL                   |
+=====================+================+========================+========================+
| `Functional tests`_ | Features       | |func-feature-pass|    | |func-feature-fail|    |
| (experimental)      +----------------+------------------------+------------------------+
|                     | Kernel         | |func-kernel-pass|     | |func-kernel-fail|     |
|                     +----------------+------------------------+------------------------+
|                     | RAS Daemon     | |func-rasdaemon-pass|  | |func-rasdaemon-fail|  |
+---------------------+----------------+------------------------+------------------------+
| `Unit tests`_       | rasdaemon      | |unit-rasdaemon-pass|  | |unit-rasdaemon-fail|  |
|                     +----------------+------------------------+------------------------+
|                     | ras-mc-ctl     | |unit-ras-mc-ctl-pass| | |unit-ras-mc-ctl-fail| |
+---------------------+----------------+------------------------+------------------------+

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
