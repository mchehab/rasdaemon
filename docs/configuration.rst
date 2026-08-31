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

IPMI BMC reporting
------------------

RAS Daemon has some support to use IPMI and OpenBMC.

IPMI is the management protocol used to access a BMC's System Event Log
(SEL).

OpenBMC is a BMC firmware that implements IPMI, Redfish, and other
interfaces.

The current support assumes OpenBMC IPMI support for SEL.

SEL consumers require a local IPMI device (``/dev/ipmi0``, ``/dev/ipmi/0``,
or ``/dev/ipmidev/0``) and a successful ``ipmitool sel`` command with a
``Version`` field. They remain disabled unless individually selected.

``OPENBMC_UNIFIED_SEL_ENABLE=yes`` enables the OpenBMC unified SEL consumer.
It creates OpenBMC-specific OEM record type ``0xfb`` entries and must be used
only with BMC firmware that supports that record format.

``AMPERE_OEM_SEL_ENABLE=yes`` enables Ampere's separate OEM-specific
AER SEL format. It embeds Ampere's IANA enterprise number and must be
used only with BMC firmware that expects it.

Each reported AER event consumes a SEL entry, so monitor the available SEL
space when enabling an IPMI consumer.

Both settings default to ``no``.

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
