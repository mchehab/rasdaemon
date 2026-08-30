.. SPDX-License-Identifier: GPL-2.0-only

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

   * - Perl command options
     - Python command options
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
