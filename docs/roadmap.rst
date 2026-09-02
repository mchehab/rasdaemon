.. SPDX-License-Identifier: GPL-2.0-only

Release maturity roadmap
------------------------

An incremental path avoids presenting aspirational requirements as current
capability:

1) **Baseline:** assign product owners, replace all qualification placeholders,
   record current results, and publish the supported configuration matrix.
2) **Engineering gates:** add sanitizer/static-analysis jobs, malformed-event
   tests, deterministic idle-resource and synthetic-burst measurements, and
   dependency fault injection.
3) **Operational readiness:** specify degraded modes, implement health and loss
   visibility, document database recovery and upgrades, and validate systemd
   hardening for each packaged feature profile.
4) **Platform qualification:** execute endurance, overload, active-action,
   compatibility, upgrade/rollback, and operator task tests on representative
   systems.
5) **Release governance:** archive the traceability report, dependency inventory,
   known risks, and signed approval for every industry-targeted release.
