.. SPDX-License-Identifier: GPL-2.0-only

Design requirements
-------------------

The keywords **shall**, **should**, and **may** indicate mandatory, recommended,
and optional requirements respectively. Values in angle brackets are product
decisions that must be set in a release qualification plan; leaving them
unset means the requirement cannot pass.

Functional requirements
~~~~~~~~~~~~~~~~~~~~~~~

1) The daemon shall detect and decode every trace event enabled in the selected
   build for which the running kernel exposes a compatible tracepoint.

2) Each decoded event shall be delivered exactly once to each enabled in-process
   consumer unless a documented fatal process failure interrupts dispatch.

3) When recording is enabled, the selected backend shall either commit an event
   or emit a diagnostic that identifies the backend, operation, and failure;
   it shall not report a failed write as successful.

4) Platform-changing actions shall be disabled by default unless the installed
   configuration explicitly enables them. Their scope, threshold, and limit
   shall be validated before the first action.

5) Configuration errors shall identify the setting and invalid value and shall
   cause either a safe fallback explicitly documented for that setting or a
   startup failure before event processing begins.

Quality requirements
~~~~~~~~~~~~~~~~~~~~

6) During an idle observation of ``<duration>``, on the named reference
   platform and with the qualified feature set, the daemon shall remain below
   the ``<idle CPU budget>`` and ``<resident-memory budget>`` and shall exhibit
   no continuing resident-memory, thread, or file-descriptor growth beyond the
   approved measurement tolerance.

7) During the qualified synthetic burst of ``<event count>`` events over
   ``<duration>``, the daemon shall remain responsive and shall lose zero
   reported events inside its controlled pipeline. Kernel-side loss, if
   observable, shall be reported separately. The qualification plan shall
   define the event mix, enabled consumers, database mode, and reference
   platform. This is a resilience test for exceptional conditions, not a
   sustained high-throughput or hard-real-time requirement.

8) Decoder input boundaries, missing fields, unknown enumeration values, and
   truncated records shall be tested. Invalid input shall not cause memory
   corruption or an undocumented process termination.

9) Failure or absence of an optional reporter or action shall not prevent core
   event monitoring unless the operator has configured that component as
   mandatory.

10) Database unavailability behavior shall be defined as one of fail-fast,
    bounded retry, or degraded no-record mode. Retry count, timeout, backoff,
    buffering, and event-loss behavior shall be configurable or documented.

11) Startup, shutdown, event-source enablement, consumer enablement, database
    connectivity, dropped events, and action failures shall produce diagnostics
    with stable severity and component identification.

12) The deployment shall expose a documented health check that distinguishes
    process running, trace source active, and persistence healthy states.

13) A release shall include a threat model covering root execution, tracefs and
    debugfs input, configuration and environment input, trigger execution,
    database credentials and transport, local sockets, IPMI access, and files
    created by the daemon.

14) The packaged service shall apply the strongest systemd sandboxing compatible
    with the qualified feature set. Any required capability, writable path,
    device, namespace, or sandbox exception shall have a documented rationale
    and a regression test or deployment check.

15) Secrets shall not be written to logs or command output. Remote database
    deployments used for qualification shall authenticate peers and protect
    credentials and event data in transit according to deployment policy.

16) Each containment or isolation action shall define prerequisites, affected
    resources, maximum action rate, failure behavior, operator-visible evidence,
    and recovery or rollback procedure. Qualification shall use the target
    platform or an accepted platform simulator.

17) Continuous integration shall build the minimum and full supported feature
    sets, compile with every supported compiler family, run hermetic tests, and
    run each supported database integration suite.

18) Release candidates shall pass AddressSanitizer and UndefinedBehaviorSanitizer
    tests. Event-parser fuzzing, static analysis, and coverage shall use release
    thresholds defined in the qualification plan.

19) A new event producer or consumer shall use the registry interfaces, document
    payload ownership and failure behavior, include positive and malformed-input
    tests, and introduce no unconditional dependency when its feature is
    disabled.

20) The release shall publish a tested compatibility matrix covering kernel,
    libtraceevent, compiler, architecture, distribution or libc, database client,
    and database server versions. Entries shall distinguish build-only from
    executed tests.

21) Upgrade and rollback shall preserve configuration and previously recorded
    data across all supported version transitions. Schema changes shall be
    versioned, atomic where supported, and covered by forward and rollback tests.

22) Representative operators shall complete installation, first event capture,
    database verification, fault diagnosis, and safe shutdown using published
    documentation. The qualification plan shall set task-success and time limits
    and record observed documentation defects.

23) The qualification plan shall set proportionate budgets for idle CPU,
    resident-memory stability, startup time, shutdown drain time, synthetic-burst
    handling, and database growth on a named reference platform. Event-processing
    latency need only be bounded sufficiently to keep the daemon responsive and
    prevent uncontrolled backlog during the qualified burst. Automated
    measurements shall fail when a budget regresses beyond the approved
    tolerance.

24) Each release shall publish its source revision, dependency inventory or
    SBOM, build provenance, known limitations, security-reporting route, and
    support status. Artifact signing and support duration shall follow the
    distributor's stated release policy.
