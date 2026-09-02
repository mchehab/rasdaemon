.. SPDX-License-Identifier: GPL-2.0-only

Verification and traceability
-----------------------------

Each release qualification report should contain this evidence:

.. list-table:: Minimum verification matrix
   :header-rows: 1
   :widths: 24 28 48

   * - Requirement area
     - Verification method
     - Required evidence
   * - Functional behavior
     - Unit and integration test
     - Trace fixtures, consumer call counts, SQL failure tests, configuration
       validation tests, and action default-state tests
   * - Reliability and availability
     - Stress, endurance, and fault-injection test
     - Named workload and platform, event counts at every boundary, resource
       graphs, forced dependency failures, and recovery result
   * - Serviceability
     - Operational scenario review
     - Log samples, health-check results, diagnostic catalog, and runbook links
   * - Security and safety
     - Threat analysis, automated analysis, and platform test
     - Threat model, mitigations, residual-risk acceptance, sanitizer/static
       analysis reports, service security score, and action qualification log
   * - Testability and maintainability
     - CI and design review
     - Test results, coverage, compiler/configuration matrix, API documentation,
       and review checklist
   * - Portability and deployment
     - Compatibility, installation, upgrade, and rollback test
     - Executed platform matrix, package results, preserved configuration, and
       database schema/data comparison
   * - Usability
     - User-centered task test
     - Participant role, scenario, completion rate, elapsed time, errors, and
       resulting documentation changes
   * - Performance and resource usage
     - Idle observation and repeatable synthetic-burst test
     - Reference hardware/software, feature and database configuration, sampling
       interval, CPU time, resident-memory high-water mark and trend, thread and
       file-descriptor counts, event counts at controlled boundaries, workload
       generator, raw results, baseline, variance, and pass/fail decision
   * - Supportability
     - Release audit
     - Versioned artifacts, SBOM/provenance, signatures where required, release
       notes, support statement, and known limitations

Evidence shall identify the source revision, build options, compiler, kernel,
dependencies, test environment, commands, results, and retained logs. A
requirement may be marked ``Pass``, ``Fail``, ``Not tested``, or ``Not
applicable``. ``Not applicable`` requires an approved rationale. Release gates
shall not treat ``Not tested`` as ``Pass``.

Performance evidence shall be interpreted according to rasdaemon's workload.
RAS events are exceptional, so idle resource stability and correct behavior
during a bounded burst are release concerns. Sustained high-throughput or
hard-real-time latency is not a product objective unless a distributor adds such
a requirement for a particular deployment.
