.. SPDX-License-Identifier: GPL-2.0-only

.. |le| unicode:: U+2264
.. |gt| unicode:: U+003E

============================
Migrating from version 0.8.5
============================

RAS Daemon up to version 0.8.5 had ``ras-mc-ctl`` written in Perl.
Such tool was rewritten in Python to bring several new features, specially
when dealing with database.

This chapter describes the difference between the previous version and
its current implementation.

.. _ras-mc-ctl-dimm-migration:

Migrating ras-mc-ctl DIMM commands
==================================

The Python implementation of ``ras-mc-ctl`` places all EDAC and DIMM
operations below the ``dimm`` command.  ``mem`` is an alias for ``dimm``.
The old Perl implementation accepted these options directly at the top
level, so most command lines need only the new command name.

For example, replace::

   $ sudo ras-mc-ctl --status

with::

   $ sudo ras-mc-ctl dimm --status

DIMM command mapping
--------------------

The following table maps every DIMM operation provided by the Perl tool.

.. list-table::
   :header-rows: 1
   :widths: 31 42 27

   * - ras-mc-ctl |le| v0.8.5 (Perl)
     - ras-mc-ctl |gt| v0.8.5 (Python)
     - Notes
   * - ``--mainboard``
     - ``dimm --mainboard``
     - Prints the detected vendor and model.
   * - ``--mainboard=VENDOR:MODEL``
     - ``dimm --mainboard --vendor VENDOR --model MODEL``
     - Quote vendor or model values containing spaces.
   * - ``--status``
     - ``dimm --status``
     - Returns failure when no EDAC driver is loaded.
   * - ``--print-labels``
     - ``dimm --print-labels``
     - Compares configured and sysfs labels.
   * - ``--guess-labels``
     - ``dimm --guess-labels``
     - Reads Locator and Bank Locator from DMI.
   * - ``--register-labels``
     - ``dimm --register-labels``
     - Writes labels to EDAC sysfs.
   * - ``--register-labels --delay=N``
     - ``dimm --register-labels --delay N``
     - ``--delay`` is rejected without ``--register-labels``.
   * - ``--labeldb=FILE --print-labels``
     - ``dimm --labeldb FILE --print-labels``
     - Selects an alternate primary label database.
   * - ``--layout --human``
     - ``dimm --layout``
     - Sizes are scaled automatically.
   * - ``--layout (without --human)``
     - ``dimm --layout``
     - There is no **non-human** option in the Python tool. The logic
       will automatically move from MB to GB, TB, ... as the memory
       size grows.
   * - ``--error-count``
     - ``dimm --error-count``
     - Consolidates ranks which have the same label.
   * - ``--error-count --per-rank``
     - ``dimm --error-count --per-rank``
     - Displays each EDAC location separately.
   * - ``--quiet --status``
     - ``dimm --quiet --status``
     - Suppresses DIMM status and diagnostic messages.
   * - ``--help``
     - ``dimm --help``
     - Shows the DIMM-specific command reference.

Several DIMM actions may still be requested together.  For example::

   $ sudo ras-mc-ctl dimm --mainboard --print-labels --error-count

They run in the same order as in the Perl implementation.  A failure in any
requested action makes the command exit unsuccessfully.  Ordinary per-action
failures do not prevent the remaining actions from being attempted.

DIMM and database operations
----------------------------

The Perl tool allowed DIMM and database reports in one invocation.  The
Python command groups are deliberately separate.  Replace a mixed command
such as::

   $ sudo ras-mc-ctl --status --summary

with two commands::

   $ sudo ras-mc-ctl dimm --status
   $ sudo ras-mc-ctl db --summary

Mainboard detection and overrides
---------------------------------

Automatic mainboard detection continues to support
``${sysconfdir}/ras/mainboard``.  A static override contains ``vendor`` and
``model`` assignments::

   vendor = Example Computer Corporation
   model = Example Server 2000

Alternatively, the file may name a helper::

   script = /usr/libexec/ras-mainboard

The helper must write the same ``vendor=...`` and ``model=...`` assignments
to standard output.  As with the Perl tool, the configured command is run by
the shell, so the mainboard file must only be writable by trusted
administrators.

When no complete configured override is available, the tool reads DMI data
from sysfs and then falls back to ``dmidecode``.  The new ``--dmidecode``
option bypasses the configuration file and sysfs detection.  The new
``--vendor`` and ``--model`` options override individual detected values;
specify both when a complete manual identity is desired::

   $ sudo ras-mc-ctl dimm --print-labels \
       --vendor "Example Computer Corporation" --model "Example Server 2000"

Label database paths
--------------------

The default files continue to reside below the configured system directory:

* ``${sysconfdir}/ras/dimm_labels.db`` is the primary label database;
* files in ``${sysconfdir}/ras/dimm_labels.d/`` extend that database;
* ``${sysconfdir}/ras/mainboard`` optionally overrides board detection.

``${sysconfdir}`` is normally ``/etc``, but follows the value selected when
rasdaemon is built and installed.  ``--labeldb FILE`` replaces only the
primary database; the configured ``dimm_labels.d`` directory is still read.

Exit status and quiet operation
-------------------------------

Successful DIMM operations exit with status zero.  A missing EDAC driver,
missing DIMMs, invalid counters, failure to obtain DMI data required by an
operation, or another failed requested operation produces a nonzero status.
Invalid option combinations, such as ``--delay`` without
``--register-labels`` or ``--per-rank`` without ``--error-count``, are
rejected before any action runs.

``dimm --quiet`` suppresses runtime DIMM status and diagnostic messages.  It
does not suppress command-line syntax errors or data explicitly requested by
options such as ``--layout`` or ``--error-count``.

.. _ras-mc-ctl-database-migration:

Migrating ras-mc-ctl database commands
======================================

The Python implementation places all database operations below the
``database`` command. ``db`` is its shorter alias. For example, replace::

   $ sudo ras-mc-ctl --summary

with::

   $ sudo ras-mc-ctl db --summary

Database command mapping
------------------------

The following table maps every database operation provided by the Perl tool.

.. list-table::
   :header-rows: 1
   :widths: 30 43 27

   * - ras-mc-ctl |le| v0.8.5 (Perl)
     - ras-mc-ctl |gt| v0.8.5 (Python)
     - Notes
   * - ``--summary``
     - ``db --summary``
     - Uses event-specific groupings and separates remote records by hostname.
   * - ``--errors``
     - ``db --errors``
     - Known EXTLOG, CXL and NVIDIA values are decoded in text output.
   * - ``--since DATE --summary``
     - ``db --since DATE --summary``
     - The date is interpreted in the local timezone of the client.
   * - ``--since DATE --errors``
     - ``db --since DATE --errors``
     - Applies the same inclusive lower date boundary.
   * - ``--vendor-errors-summary KunPeng9xx``
     - ``db --summary --table 'hip08_*_event_v2' --table hisi_common_section_v2``
     - Uses the registered grouping for each selected HiSilicon table.
   * - ``--vendor-errors KunPeng9xx``
     - ``db --errors --table 'hip08_*_event_v2' --table hisi_common_section_v2``
     - Displays records from the corresponding autodiscovered tables.
   * - ``--vendor-errors KunPeng9xx MODULE``
     - Add ``--module MODULE``
     - Alias for ``--where 'module_id~=MODULE OR sub_module_id~=MODULE'``.
   * - ``--vendor-errors-summary YiTian7XX``
     - ``db --summary --table yitian_ddr_reg_dump_event``
     - Groups the DDR register dumps by address.
   * - ``--vendor-errors YiTian7XX``
     - ``db --errors --table yitian_ddr_reg_dump_event``
     - Displays detailed DDR register-dump records.
   * - ``--vendor-errors-summary CorsicaDpu1xx``
     - ``db --summary --table jm_payload0_event``
     - Groups records by severity and subsystem.
   * - ``--vendor-errors CorsicaDpu1xx``
     - ``db --errors --table jm_payload0_event``
     - Displays detailed CorsicaDpu records.
   * - ``--vendor-errors CorsicaDpu1xx MODULE``
     - ``db --errors --table jm_payload0_event --module MODULE``
     - Selects ``module_id`` case-insensitively through the common alias.
   * - ``--vendor-platforms``
     - ``db --list-tables``
     - Static platform identifiers are replaced by discovered table names.
   * - ``--help``
     - ``db --help``
     - Shows the database-specific command reference.

Summary reports
---------------

``db --summary`` retains the event-specific purpose of the Perl report while
discovering the available tables at runtime. Known tables are grouped as
follows:

.. list-table::
   :header-rows: 1
   :widths: 36 64

   * - Event table
     - Summary fields
   * - ``mc_event``
     - Error type, DIMM label and EDAC location
   * - ``aer_event``
     - Error type and message
   * - ``arm_event``
     - MPIDR
   * - NVIDIA tables
     - Signature and socket
   * - CXL tables
     - Memory device
   * - ``extlog_event``
     - Decoded error type and severity
   * - ``devlink_event``
     - Device name
   * - ``disk_errors``
     - Device
   * - ``memory_failure_event``
     - Action result
   * - ``mce_record``
     - Error message
   * - ``signal_event``
     - Signal code
   * - KunPeng OEM and common tables
     - Severity and module
   * - KunPeng PCIe local table
     - Severity and submodule
   * - ``yitian_ddr_reg_dump_event``
     - Address
   * - ``jm_payload0_event``
     - Severity and subsystem
   * - Other discovered tables
     - Hostname and table count

The Python tool also provides database reports which have no Perl equivalent:

* ``db --table-summary`` counts events by hostname and table;
* ``db --errors-per-table`` lists counts for non-empty tables;
* ``db --count`` supports configurable grouping, filtering and ordering;
* ``db --until`` supplies an inclusive upper date boundary;
* ``db --hostname`` selects one host in a remote database;
* ``db --json`` produces machine-readable output.

Database backends and timestamps
--------------------------------

The Perl tool read one fixed SQLite database. The Python command reads the
backend and connection parameters from the rasdaemon configuration and
supports SQLite, MySQL/MariaDB and PostgreSQL. SQLite continues to use the
local timestamps recorded on its single host. MySQL and PostgreSQL store UTC
timestamps so records from hosts in different timezones can be combined;
``ras-mc-ctl`` displays and filters them in the local timezone of the client.
