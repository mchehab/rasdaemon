.. SPDX-License-Identifier: GPL-2.0-only

Developer Guide
===============

Architecture
------------

rasdaemon is assembled from self-registering modules. Meson decides which
source files enter the build, so disabling a feature removes its module and
its dependencies rather than leaving runtime stubs throughout the event loop.
Module descriptors have static lifetime and register with
``REGISTER_RAS_MODULE``.

The registry initializes modules in dependency order:

#. ``DB_MODULE`` selects and initializes database backends.
#. ``BASE_EVENT_MODULE`` owns base trace-event decoders and their tables.
#. ``SUB_EVENT_MODULE`` contains decoders which depend on base events.
#. ``ACTIONS_MODULE`` consumes decoded events to apply policy or report them.

Cleanup visits those levels in reverse order. A successful module initializer
must leave all owned state reachable from its ``ras_module_ctx`` and its
cleanup callback must release that state. Modules must not call optional
modules directly; registration and published events provide the boundary.

Events and consumers
--------------------

A trace-event producer defines a static ``ras_event_entry`` and registers it
with ``REGISTER_RAS_EVENT``. The entry describes the tracepoint, decoder,
optional filter and preparation callbacks, decoded payload identifier,
database recorder, and unit test. The producer owns the decoded payload and
publishes it synchronously with ``ras_event_publish``.

Actions, reporters, and database persistence use static
``ras_event_consumer`` descriptors registered with
``REGISTER_RAS_EVENT_CONSUMER``. Consumers select payload identifiers with an
event bitmap. Lower numeric priorities run first; equal-priority consumers are
ordered by name. A consumer must not retain the borrowed payload after its
callback returns.

Database ownership
------------------

Multiple SQL backends may be compiled, but only one is selected for a daemon
process. Event modules which persist data own a static ``db_desc_and_stmt``
pair. They register it with ``ras_db_table_register`` during initialization
and unregister it during cleanup. The descriptor and statement pair must
remain valid while registered.
The database session opens registered tables and finalizes their prepared
statements; module cleanup must unregister only after its statements are no
longer in use.

Code which can build without database support should include ``db/ras-db.h``
and use its no-database inline implementations. Avoid spreading ``HAVE_DB``
conditionals through event modules.

Tests
-----

Event-handler tests belong to their ``ras_event_entry`` through the
``test_group``, ``test``, and ``test_priority`` fields. This keeps a decoder,
its build selection, and its tests together. Tests which do not correspond to
one trace-event descriptor, including core, module-registry, action, generic
database, backend integration, ERST, and vendor-decoder tests, register with
``REGISTER_TEST``.

List the groups present in a configured build and run one with::

   $ ./build/unittest --list-groups
   $ ./build/unittest --group events

The default Meson suite is hermetic. The ``mysql`` and ``postgresql`` groups
use real servers and are intentionally run only by environments which have
provisioned their test databases.

Adding a feature
----------------

When adding an event or action:

#. Put the implementation in the appropriate Meson feature and architecture
   source list.
#. Define static module, event, and consumer descriptors as needed.
#. Keep ownership explicit: allocate in initialization, and unwind it in
   reverse order from cleanup and failure paths.
#. Register database tables through the module context instead of a central
   table list.
#. Attach event tests to event descriptors and register only standalone suites
   with ``REGISTER_TEST``.
#. Document public and internal C interfaces using Linux kernel-doc notation
   and add public APIs to :doc:`api`.

Validate the normal build with ``make`` and
``meson test -C build --print-errorlogs``. Also test the affected unit-test
groups and relevant feature-disabled or architecture-selected configurations.
The Sphinx build treats warnings as errors, so documentation and kernel-doc
warnings must be fixed before submitting a change.
