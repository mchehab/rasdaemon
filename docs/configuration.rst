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

RAS Daemon can report PCIe AER events to a BMC through IPMI.

IPMI is the management protocol used to access a BMC's System Event Log
(SEL).

OpenBMC is BMC firmware that can implement IPMI, Redfish, and other
interfaces. It does not define one universal OEM SEL format.

The ``bmc-generic``, ``ampere-oem-sel``, and ``openbmc-unified-sel`` Meson
features build the generic IPMI, Ampere OEM, and Facebook/Meta Unified SEL
consumers, respectively.

SEL consumers require a local IPMI device (``/dev/ipmi0``, ``/dev/ipmi/0``,
or ``/dev/ipmidev/0``) and a successful ``ipmitool sel`` command with a
``Version`` field. They remain disabled unless individually selected.

``BMC_GENERIC_ENABLE=yes`` enables the generic BMC consumer. It writes
standard IPMI system-event SEL records using the Critical Interrupt sensor:
corrected, non-fatal, and fatal AER events map to Bus Correctable, Bus
Uncorrectable, and Bus Fatal events. The PCI bus and device/function occupy
the event's OEM data bytes. Enable it only after confirming that the target
BMC accepts locally added SEL entries.

``OPENBMC_UNIFIED_SEL_ENABLE=yes`` enables the Facebook/Meta Unified SEL
consumer. It writes non-timestamped OEM record type ``0xfb`` entries. Enable
it only on BMC firmware that includes ``fb-ipmi-oem`` Unified SEL support;
OpenBMC branding alone is not sufficient.

``AMPERE_OEM_SEL_ENABLE=yes`` enables Ampere's OEM AER SEL consumer. It
writes timestamped OEM record type ``0xc0`` entries, with Ampere's IANA
Private Enterprise Number 52538 and Ampere-defined PCIe location data. Enable
it only on BMC firmware that implements this payload. The public Ampere
documentation does not identify a compatible model list.

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
