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
