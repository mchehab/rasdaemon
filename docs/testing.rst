.. SPDX-License-Identifier: GPL-2.0-only

Testing
=======

Unit tests
----------

rasdaemon provides cmocka groups for core data structures, trace-event
handlers, optional feature modules, vendor decoders, and database backends.
Event-handler tests use scripted trace records, so they do not require a
kernel tracefs instance or injected hardware errors.

Install ``libcmocka-devel`` or ``libcmocka-dev`` to enable the tests. Build and
run the hermetic suite with::

   $ make
   $ meson test -C build --print-errorlogs

List the groups present in the selected Meson configuration or run one group
directly with::

   $ ./build/unittest --list-groups
   $ ./build/unittest --group events

SQLite is included in the hermetic Meson suite. Its group can also be run
directly::

   $ ./build/unittest --group sqlite3

The MySQL/MariaDB and PostgreSQL groups use real database servers and must be
run explicitly after provisioning their test databases::

   $ ./build/unittest --group mysql
   $ ./build/unittest --group postgresql

Error injection
---------------

``contrib/edac-fake-inject`` injects EDAC errors when the kernel was built with
``CONFIG_EDAC_DEBUG`` and an EDAC driver is running. Run it as root while
rasdaemon is active::

   # contrib/edac-fake-inject

External injection tools are available for other error sources:

* MCE: https://git.kernel.org/pub/scm/utils/cpu/mce/mce-inject.git
* APEI: https://git.kernel.org/pub/scm/linux/kernel/git/gong.chen/mce-test.git/
* AER: https://git.kernel.org/pub/scm/linux/kernel/git/gong.chen/aer-inject.git/

Performance and resource usage
------------------------------

RAS events normally occur only under exceptional conditions. rasdaemon is not
a sustained high-throughput or hard-real-time component. Performance testing
therefore checks that an idle daemon has low, stable resource use and that it
remains correct and responsive during a bounded synthetic event burst.

Idle observation
~~~~~~~~~~~~~~~~

An already-running daemon can provide useful long-duration evidence without
being restarted. First identify exactly one process and record its command line,
start time, elapsed time, CPU time, resident memory, thread count, and open file
descriptor count. Replace ``PID`` below with the selected numeric process ID::

   # ps -p PID -o pid,lstart,etime,time,pcpu,rss,vsz,nlwp,args
   # grep -E '^(Name|State|VmRSS|VmHWM|VmData|Threads):' /proc/PID/status
   # ls -1 /proc/PID/fd | wc -l
   # cat /proc/PID/smaps_rollup

Access to ``smaps_rollup`` may require the same user as the daemon or root. It
contains more detail than is normally needed; retain at least ``Rss``, ``Pss``,
``Private_Clean``, ``Private_Dirty``, and ``Swap``. Do not publish database
credentials or the complete process environment with test results.

A single observation establishes resource use at one point but cannot prove
that memory or descriptor use is stable. For trend evidence, take timestamped
samples at a fixed interval without restarting the process. For example::

   # while kill -0 PID 2>/dev/null; do
   >     date --iso-8601=seconds
   >     ps -p PID -o etimes=,time=,pcpu=,rss=,vsz=,nlwp=
   >     grep -E '^(VmRSS|VmHWM|VmData|Threads):' /proc/PID/status
   >     find /proc/PID/fd -mindepth 1 -maxdepth 1 -printf '.' | wc -c
   >     sleep 3600
   > done | tee rasdaemon-idle-samples.txt

Stop the sampling loop with ``Ctrl-C``; this does not stop rasdaemon. Use a
shorter interval for an engineering investigation and a longer interval for an
endurance run. The qualification record shall also identify the source
revision, kernel, architecture, build options, command line, selected database
backend, database location type (local or remote), enabled consumers, sampling
interval, and whether RAS events occurred during the observation.

Interpret cumulative CPU time over the complete elapsed time rather than a
single rounded ``%CPU`` value. Assess memory, threads, and file descriptors as
time series. A high-water mark alone does not demonstrate continuing growth.
Environmental cache effects and database client behavior shall be distinguished
from allocations owned by rasdaemon where practical.

Synthetic burst observation
~~~~~~~~~~~~~~~~~~~~~~~~~~~

Burst qualification shall use recorded trace fixtures, a safe kernel error
injection facility, or an accepted simulator. It shall never inject a real host
hardware error merely to produce a benchmark. Record the event source and mix,
events offered and observed at every controlled boundary, enabled consumers,
database mode, elapsed processing time, peak resource use, diagnostics, and any
reported kernel-side loss.

Run at least the no-record and qualified database-recording configurations. A
test passes when the daemon stays responsive, its controlled pipeline accounts
for every event, resource use remains bounded, and any loss outside that
pipeline is visible. The purpose is to detect correctness or resource
regressions during exceptional bursts, not to maximize an events-per-second
score.
