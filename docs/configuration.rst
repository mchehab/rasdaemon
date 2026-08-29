.. SPDX-License-Identifier: GPL-2.0-only

Runtime Configuration
=====================

The compiled configuration path is normally ``/etc/sysconfig/rasdaemon`` for
system packages. Use ``--config FNAME`` to select another file. Entries use
``NAME=value`` syntax. Values already exported in the process environment are
not overwritten by the file.

Runtime reload is not supported; restart rasdaemon after changing the file.
The installed file is the authoritative template for the features included by
a package.

Disabling events
----------------

``DISABLE`` is a comma- or whitespace-separated list of exact trace-event
names in ``group:event`` form. For example::

   DISABLE="ras:mc_event ras:aer_event"

Database selection and connection settings are described in :doc:`databases`.

Corrected-error actions
-----------------------

When the corresponding features are built, rasdaemon can account for
corrected errors and isolate failing memory pages, memory rows, or CPUs.
Relevant settings include:

* ``PAGE_CE_ACTION``, ``PAGE_CE_THRESHOLD``, and
  ``PAGE_CE_REFRESH_CYCLE``
* ``ROW_CE_ACTION``, ``ROW_CE_THRESHOLD``, and ``ROW_CE_REFRESH_CYCLE``
* ``CPU_ISOLATION_ENABLE``, ``CPU_CE_THRESHOLD``,
  ``CPU_ISOLATION_CYCLE``, and ``CPU_ISOLATION_LIMIT``

Page and row actions accept ``off``, ``account``, ``soft``, ``hard``, and
``soft-then-hard``. The installed configuration template documents the
accepted threshold and time units.

Triggers and statistics
-----------------------

Trigger programs are resolved relative to ``TRIGGER_DIR``. These variables
select a program for each supported event type:

* ``MC_CE_TRIGGER`` and ``MC_UE_TRIGGER``
* ``AER_CE_TRIGGER`` and ``AER_UE_TRIGGER``
* ``MEM_FAIL_TRIGGER``

The configured program must be accessible to the daemon. rasdaemon supplies
event data to it through environment variables.

``MC_CE_STAT_THRESHOLD`` controls corrected-memory-error statistics, and
``POISON_STAT_THRESHOLD`` controls poison-page statistics. ``ERST_DELETE``
controls deletion of processed ERST records. Refer to the installed template
for defaults and units.
