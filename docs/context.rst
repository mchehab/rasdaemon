.. SPDX-License-Identifier: GPL-2.0-only

System context
--------------

rasdaemon is a privileged host daemon that reads Linux kernel RAS trace
events, decodes them, and reports them to system logging. Events can also be
stored in a persistent database, either locally or remotely, using a backend.
Currently available database backends are SQLite, MySQL/MariaDB, or PostgreSQL.

Optional consumers can apply containment or isolation policies and/or
report selected events to other platform services.

The principal stakeholders are:

``Platform operator``
   Installs, configures, monitors, and upgrades the daemon. The operator needs
   predictable startup, actionable diagnostics, safe defaults, and recovery
   procedures.

``Reliability engineer``
   Uses event history to diagnose hardware faults and trends. This user needs
   accurate decoding, timestamps, hardware location information, and a stable
   query interface.

``Platform integrator or distributor``
   Selects features, database backends, architecture support, service policy,
   and packaging. This user needs build-time isolation, compatibility
   information, and a verifiable release process.

``Developer or maintainer``
   Adds trace-event decoders, actions, and backends. This user needs explicit
   interfaces, ownership rules, focused tests, and useful failure diagnostics.

``Security or safety assessor``
   Evaluates privilege, external inputs, active mitigation features, data
   handling, and failure modes. This user needs a threat model, traceable
   controls, and evidence from testing.

``External entities``
   May receive reports, such as ABRT reports, to support software quality
   monitoring, capability management, and planned system or component
   decommissioning.

The Linux kernel trace-event ABI and optional database or BMC interfaces are
external dependencies. Statistical failure prediction, fleet analytics,
kernel error injection, and operation of downstream deployments remain outside
the daemon's current scope; see :doc:`overview` and :doc:`testing`.
