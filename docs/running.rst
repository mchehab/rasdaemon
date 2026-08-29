.. SPDX-License-Identifier: GPL-2.0-only

Running rasdaemon
=================

The daemon generally requires root permission to read kernel tracing nodes.
It prefers a mounted tracefs filesystem and falls back to the legacy
``debugfs/tracing`` location. When trace instances are supported, it creates
or reuses the ``rasdaemon`` instance.

Run rasdaemon in the background with::

   # rasdaemon

Events are sent to syslog or journald. To remain in the foreground and print
events on the console, use::

   # rasdaemon --foreground

To persist errors, compile at least one database backend and add
``--record``::

   # rasdaemon --foreground --record

The backend defaults to SQLite and can be selected with
``RASDAEMON_DB_BACKEND`` in the configuration file or process environment.
See :doc:`databases` for backend setup and :doc:`configuration` for runtime
settings.

rasdaemon can also be started through systemd::

   # systemctl start rasdaemon

Offline MCE decoding
--------------------

When x86 MCE support is compiled in, rasdaemon can post-process an event from
raw machine-check register values. For an AMD SMCA event, for example::

   # rasdaemon --post-processing --status STATUS --ipid IPID --smca \
       --family FAMILY --model MODEL --bank BANK

``STATUS`` and ``IPID`` are mandatory hexadecimal register values. ``--smca``,
``--family``, and ``--model`` are needed when the event is not being decoded
on the originating system. ``--bank`` is optional.

Use ``rasdaemon --help`` to see the options compiled into the installed
binary.
