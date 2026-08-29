.. SPDX-License-Identifier: GPL-2.0-only

Contributing
============

rasdaemon follows the Linux kernel coding style and submission conventions.
The module, event, database, and test-registration contracts are described in
:doc:`development`.

Before submitting a change, run the relevant unit-test groups, the hermetic
suite, and the warning-fatal documentation build::

   $ make
   $ meson test -C build --print-errorlogs

Check each patch with::

   $ scripts/checkpatch.pl --strict --no-tree PATCH

Submitting changes
------------------

The preferred contribution path is a pull request against
https://github.com/mchehab/rasdaemon/. Alternatively, submit a merge request
against https://gitlab.com/mchehab_kernel/rasdaemon.

Patches may also be sent to ``linux-edac@vger.kernel.org`` with a copy to
Mauro Carvalho Chehab at ``mchehab@kernel.org``. When using email, please also
open a GitHub issue so the series can be tracked.

Describe the change in each commit and add a ``Signed-off-by`` line using your
real name. The sign-off certifies the contribution under the
`Developer Certificate of Origin 1.1
<https://developercertificate.org/>`_.
