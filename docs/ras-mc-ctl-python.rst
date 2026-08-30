.. SPDX-License-Identifier: GPL-2.0-only

Using ras-mc-ctl
================

``ras-mc-ctl`` inspects errors recorded by rasdaemon and reports information
about the system's EDAC memory devices. Its two command groups are
``database`` (also spelled ``db``) and ``dimm`` (also spelled ``mem``).

Users moving from the former Perl implementation can find complete DIMM and
database command mappings in :doc:`ras-mc-ctl-perl`.

The database commands require rasdaemon to have been run with ``--record``.
They use the backend and connection settings described in :doc:`databases`.
The Python SQLAlchemy package and the driver for the selected backend must
also be installed; see :doc:`installation`.

Display recorded errors
-----------------------

To display every recorded error in detail, use::

   $ sudo ras-mc-ctl db --errors

Detailed output is the default, so ``ras-mc-ctl db`` produces the same kind
of report. To summarize events using fields appropriate to each known table,
use::

   $ sudo ras-mc-ctl db --summary

The report remains table-aware when new tables are discovered. Tables without
a registered event-specific grouping are summarized by hostname and table.

For a shorter overview, count the records in each non-empty event table::

   $ sudo ras-mc-ctl db --errors-per-table

Alternatively, summarize the records by hostname and table::

   $ sudo ras-mc-ctl db --table-summary

Machine-readable JSON output
----------------------------

By default, ``ras-mc-ctl`` outputs data in human format.

There is a ``--json`` flag (currently supported only by database
command) that changes the output format to JSON. It can be combined
with any db command::

   $ sudo ras-mc-ctl db --errors --json
   $ sudo ras-mc-ctl db --summary --json
   $ sudo ras-mc-ctl db --table-summary --json
   $ sudo ras-mc-ctl db --count --table mc_event --corrected --json

On each output:

- ``format_version`` is currently set to ``1``, and it is meant to
  indicate changes to the format;
- ``mode`` indicates the type of JSON output.

- Detailed reports contain a ``records`` array with ``hostname``,
  ``timestamp``, ``table`` and ``fields`` objects.

- Count reports contain a ``groups`` array with ``values`` and ``count``
  objects.

- Event-specific summary reports contain a ``groups`` array.

- Table summary reports contain a ``hosts`` object.

Other reports like ``--list-tables``, ``--describe``, and ``--create-index``
also support JSON output.

Binary database values are represented by an object containing a ``base64"``
encoding.

Please notice that new fields may be added without changing the version,
so consumers can ignore object members they do not recognize.

The ``hostname`` column (and ``--hostname`` filter) are used with
MySQL/MariaDB and PostgreSQL. Both are ignored when using SQLite.

Discover tables and fields
--------------------------

The available tables depend on the event handlers included in the installed
rasdaemon and the schema initialized in the database. List them before
constructing a more specific query::

   $ sudo ras-mc-ctl db --list-tables

Then inspect the fields in one table::

   $ sudo ras-mc-ctl db --describe --table mc_event

``--table`` accepts an exact table name or a shell-style pattern and may be
repeated. Quote patterns to prevent the shell from expanding them. Tables can
also be omitted with a repeatable ``--except`` option. For example, describe
all HiSilicon tables except PCIe local records with::

   $ sudo ras-mc-ctl db --describe --table 'hip08_*' \
       --except hip08_pcie_local_event_v2

Filter and order database results
---------------------------------

The ``--since`` and ``--until`` options accept dates in ``YYYY-MM-DD`` form.

Dates are interpreted in the local timezone of the machine running
``ras-mc-ctl``.

Both limits are inclusive, so this command reports errors
recorded during August 2026, in local time from zero hours at the first
day until midnight of the last day (including 24:00:00)::

   $ sudo ras-mc-ctl db --errors --since 2026-08-01 --until 2026-08-31

SQLite databases keep timestamps in the local time of the rasdaemon host,
including its numeric UTC offset. MySQL and PostgreSQL store timestamps as UTC
so that a shared database can combine records from hosts in different
timezones. ``ras-mc-ctl`` converts MySQL and PostgreSQL results to the local
timezone of the machine on which it is running.

Use ``--where 'FIELD OP VALUE'`` for field comparisons. Supported operators
are ``=``, ``!=``, ``<``, ``<=``, ``>``, ``>=``, and ``~=``. The ``~=``
operator performs case-insensitive equality on text fields. The option may be
repeated; all comparisons must match. Discover valid field names with
``--describe``. For example::

   $ sudo ras-mc-ctl db --errors --table mc_event \
       --where 'label=DIMM_A1' --where 'err_count>=1'

Join alternatives with the ``OR`` keyword inside one quoted ``--where``
expression when any one may match. This is particularly useful for records
which use different field names for the same concept. For example::

   $ sudo ras-mc-ctl db --errors --table 'hip08_*_event_v2' \
       --table hisi_common_section_v2 \
       --where 'module_id~=SMMU OR sub_module_id~=SMMU'

Each ``--where`` occurrence is one parenthesized group. Separate ``--where``
options are joined with AND, so every group must match. Parentheses and an
explicit ``AND`` operator within one expression are not supported.

The common module lookup has a shorter alias. This command applies the same
case-insensitive ``module_id`` or ``sub_module_id`` predicate as the previous
example::

   $ sudo ras-mc-ctl db --errors --table 'hip08_*_event_v2' \
       --table hisi_common_section_v2 --module SMMU

Detailed reports can be limited to selected fields and sorted by one or more
fields. The following command displays the newest PCIe AER events first::

   $ sudo ras-mc-ctl db --errors --table aer_event \
       --select timestamp --select dev_name --select err_type \
       --order-by timestamp:desc

The field names in these examples are table-specific. Use ``--describe`` to
check them against the database being queried.

Count selected errors
---------------------

Use ``--count`` to count matching records. It can be combined with a severity
selector such as ``--corrected``, ``--uncorrected``, ``--deferred``,
``--fatal``, ``--info``, or ``--recoverable``::

   $ sudo ras-mc-ctl db --count --table mc_event --corrected

Add one or more ``--group-by`` options to split the count. For example, count
corrected EDAC events by DIMM label::

   $ sudo ras-mc-ctl db --count --table mc_event --corrected \
       --group-by label --order-by count:desc

Or summarize HiSilicon OEM records by severity::

   $ sudo ras-mc-ctl db --count --table 'hip08_*' \
       --group-by err_severity --order-by count:desc

For large databases, ``--create-index`` creates any missing indexes for the
selected tables. Unlike the reporting options, this modifies the configured
database and requires write permission::

   $ sudo ras-mc-ctl db --create-index --table mc_event

Inspect DIMMs and EDAC
----------------------

Check whether an EDAC kernel module is loaded with::

   $ sudo ras-mc-ctl dimm --status

Display the memory layout reported through EDAC sysfs, followed by corrected
and uncorrected error counts for each DIMM label::

   $ sudo ras-mc-ctl dimm --layout
   $ sudo ras-mc-ctl dimm --error-count

When multiple ranks have the same label, their counters are combined. To
display every rank and its EDAC location separately, add ``--per-rank``::

   $ sudo ras-mc-ctl dimm --error-count --per-rank

The DIMM commands depend on the EDAC and DMI information made available by
the kernel and firmware. Consequently, the available output varies by system.

Manage DIMM labels
------------------

Print the detected mainboard vendor and model, and compare the labels in the
rasdaemon label database with the current EDAC sysfs labels::

   $ sudo ras-mc-ctl dimm --mainboard
   $ sudo ras-mc-ctl dimm --print-labels

If automatic mainboard detection is insufficient, specify the vendor and
model explicitly::

   $ sudo ras-mc-ctl dimm --print-labels \
       --vendor Dell --model R740

``--labeldb FILE`` selects a label database instead of
``${sysconfdir}/ras/dimm_labels.db``. Files installed in
``${sysconfdir}/ras/dimm_labels.d/`` are also read. ``${sysconfdir}`` is
normally ``/etc``. Use ``--guess-labels`` to print the Locator and Bank
Locator pairs obtained from DMI::

   $ sudo ras-mc-ctl dimm --guess-labels

After checking the proposed labels with ``--print-labels``, load them into the
EDAC driver with::

   $ sudo ras-mc-ctl dimm --register-labels

This command writes the labels to EDAC sysfs and therefore requires root
permission. ``--delay SECONDS`` can postpone those writes when EDAC devices
need additional time to appear during system startup.

Configuration and help
----------------------

Global options precede the command group. Use ``--config`` to read backend
settings from an alternate rasdaemon environment file::

   $ sudo ras-mc-ctl --config /path/to/rasdaemon.conf db --summary

For the complete options supported by the installed version, run::

   $ ras-mc-ctl --help
   $ ras-mc-ctl db --help
   $ ras-mc-ctl dimm --help

The same command reference is available in the ``ras-mc-ctl(8)`` manual page.
