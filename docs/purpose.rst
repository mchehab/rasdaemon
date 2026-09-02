.. SPDX-License-Identifier: GPL-2.0-only

Purpose and status
------------------

RAS Daemon was designed to be a production ready daemon to detect RAS
events caused by hardware problems. As such extra care was taken to avoid
increasing memory consumption during its runtime.

It assesses the lifecycle qualities that make the daemon suitable for
integration into production Linux platforms: reliability, availability,
serviceability, security, testability, maintainability, portability,
deployability, usability, performance, and data integrity.

The document has two purposes:

* describe the current RAS Daemon design and how it was written to ensure
  quality; and
* define measurable requirements and verification evidence needed for an
  industry-grade release decision.

Such requirements are the project targets. They do not claim certification,
compliance, or that every target is already implemented. A target is satisfied
only when the stated evidence exists for the release under assessment.
