.. SPDX-License-Identifier: GPL-2.0-only

Overview
========

RAS Daemon provides a way to collect Platform Reliability, Availability and
Serviceability (RAS) reports emitted by Linux kernel trace events. It can log
events and persist them in SQLite, MySQL/MariaDB, or PostgreSQL for later
analysis. Only one database backend is active in a daemon process.

Intended use
------------

This project provides general-purpose software components intended primarily
for integration, development, research, and infrastructure use by technical
users. It is not offered as a consumer-facing online service or managed
platform.

Goals
-----

rasdaemon was created to replace edac-tools after the addition of the Hardware
Events Report Method (HERM) patches to the Linux EDAC drivers. Its long-term
goal is to collect hardware-error events reported by sources such as EDAC,
MCE, and PCI into one common userspace framework.

The kernel does not expose EDAC memory-error counters as its primary userspace
interface. Trace events let userspace account for errors while retaining
timing and event detail. This matters because a raw count alone cannot show
whether errors are sparse, increasing over time, or concentrated in a burst.
rasdaemon therefore keeps the kernel/userspace interface simple and leaves
correlation and policy to userspace.

Sophisticated statistical data mining is outside the project's current scope.
Such analysis requires representative hardware reliability data and knowledge
of the relevant probability distributions and parameters.

rasdaemon works with kernels from version 3.5, where the HERM changes were
introduced. Kernel 3.10 or later is recommended for fuller functionality.

Error injection is also outside the daemon's primary scope. See
:doc:`testing` for the included helper and links to external injection tools.

Project resources
-----------------

The primary source repository is https://github.com/mchehab/rasdaemon/.
Mirrors are available at https://gitlab.com/mchehab_kernel/rasdaemon and
http://git.infradead.org/users/mchehab/rasdaemon.git. Release tarballs are
published at http://www.infradead.org/~mchehab/rasdaemon/.

The original HERM discussion is archived at
http://lkml.indiana.edu/hypermail/linux/kernel/1205.1/02075.html.
