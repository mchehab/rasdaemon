.. SPDX-License-Identifier: GPL-2.0-only

Contributing
============

rasdaemon follows the Linux kernel coding style and submission conventions.
The module, event, database, and test-registration contracts are described in
:doc:`development`.

Before submitting a change, run unit tests and fix any warnings. It is also
good to compile with both gcc and clang, as they may produce different
warnings. A good procedure is to run::

   # GCC compile
   make distclean && CC=gcc make

   # self-contained (hermetic) tests
   meson test -C build --print-errorlogs

   # CLANG compile
   make distclean && CC=clang make

Please notice that the full test suite also have tests that depend on
setting up MySQL or MariaDB and PostgreSQL databases. Running::

   make test

Executes all of them, but the DB backend tests will fail if you don't
setup a rasdaemon_test database. The
`Github action workflow <.github/workflows/unittest.yml>`_ contains
instructions to setup such database.

Check each patch with::

   scripts/checkpatch.pl --strict --no-tree PATCH

If the patch(es) modify python code, run also python unit tests::

   make python-test

Submitting changes
------------------

To avoid a single point of failure, the rasdaemon code is updated altogether
on three separate independent locations:

1. https://github.com/mchehab/rasdaemon/
2. https://gitlab.com/mchehab_kernel/rasdaemon
3. https://git.infradead.org/?p=users/mchehab/rasdaemon.git

Yet, to keep our workflow simpler, we currently only check contributions
or issues at `repository (1) <https://github.com/mchehab/rasdaemon/>`_.

So, the proper way to submit code changes it to open a pull request (PR)
against https://github.com/mchehab/rasdaemon/. If there are strong
technical reasons to submit code on a different way, be sure to let
me aware.

Before opening a new PR, please search for existing ones and related
issues first. Please mention related issues when opening a new PR.

If you cannot prepare a PR, or if, before doing that, some discussions are
needed, you may open an issue at https://github.com/mchehab/rasdaemon/issues.

Also, please notice that rasdaemon patches are no longer expected
or tracked by email. So, please don't send patches to the Linux EDAC mailing
list anymore.

When opening a PR, ensure that your patches are tested by
``scripts/checkpatch.pl`` and follow Linux Kernel best practices.

In particular, each patch needs a ``Signed-off-by`` line using your
real name. The ``sign-off-by`` certifies the contribution under the
`Developer Certificate of Origin 1.1 <https://developercertificate.org/>`_.
