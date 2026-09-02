.. SPDX-License-Identifier: GPL-2.0-only

Design quality summary
----------------------

rasdaemon includes design choices intended to make it suitable for long-running
operation on Linux systems. The following table summarizes those choices. It
describes the current design; measurable release targets are listed separately
in :doc:`requirements`.

.. list-table:: Design qualities
   :header-rows: 1
   :widths: 13 75

   * - Quality
     - How the design supports it
   * - Reliability
     - Event types and consumers are registered explicitly. Consumers run in a
       deterministic order, and database writes use prepared statements.
   * - Availability
     - Optional features are implemented as independent modules. A feature can
       be excluded from the build without leaving runtime stubs in the event
       path. systemd can restart the daemon after an abort.
   * - Serviceability
     - Events are decoded into readable reports, sent to syslog or journald,
       and optionally stored for later investigation. ``ras-mc-ctl`` provides
       summaries of recorded errors.
   * - Safe operation
     - Features that change platform state, such as isolation and containment
       actions, require explicit configuration. Limits and thresholds can be
       used to constrain corrective actions.
   * - Security
     - Optional actions are gated by build and runtime settings. Remote database
       backends provide connection timeouts and optional encrypted transport.
       Security issues have a documented reporting process.
   * - Testability
     - Event decoders are associated with their tests. Hermetic unit tests can
       exercise recorded trace data without injecting real hardware errors,
       while separate tests cover the supported database backends.
   * - Maintainability
     - Module, event, consumer, database, and test registries define clear
       extension points. Resource ownership and cleanup order are documented,
       and internal interfaces use kernel-doc comments.
   * - Portability
     - Build options select x86, Arm, or RISC-V support and allow optional
       dependencies to be disabled. Continuous integration builds rasdaemon on
       multiple Linux distributions.
   * - Deployability
     - The project supplies Meson installation rules, RPM packaging support,
       systemd service units, logging configuration, log rotation, and an
       installed configuration template.
   * - Efficiency
     - Kernel-side event filtering, compile-time feature selection, synchronous
       dispatch, and prepared database operations keep the normal event path
       direct and avoid unnecessary components. Because RAS events are
       exceptional, qualification emphasizes stable, low-overhead idle operation
       and bounded behavior during short bursts rather than sustained throughput
       or hard-real-time latency.

These properties provide the design foundation. Release qualification still
requires evidence from the tests and checks described in :doc:`verification`.
