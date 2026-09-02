.. SPDX-License-Identifier: GPL-2.0-only

Design solution
---------------

The design is a synchronous event pipeline with self-registering components::

   +--------------------+   +---------------------+
   | Linux trace events |   | Other event sources |
   +--------------------+   +---------------------+
           |                   |
           v                   v
         +-----------------------+
         |    Events handler     |   Dispatches events to each specific handler
         +-----------------------+
           |
           v
   +------------------------+
   | event-specific decoder |        Publishes a typed, borrowed payload
   +------------------------+
           |
           v
   +---------------------------+
   | Event consumer dispatcher |     Takes actions on event arrival
   +---------------------------+
           | | |
           | | |   +------------------------------+
           | | +-> | external reporting consumers |
           | |     +------------------------------+
           | |
           | |    +----------------------+
           | +--> | SQL database backend |
           |      +----------------------+
           |
           |    +------------------------------------------------------+
           +--> | syslog, journald, or terminal output (if foreground) |
                +------------------------------------------------------+

Meson selects the compiled feature and architecture modules. At runtime, the
module registry initializes database modules, base event modules, dependent
event modules, and actions in dependency order, and cleans them up in reverse
order.

Static event descriptors bind tracepoints to decoders, persistence, and
tests.

Static consumer descriptors subscribe to decoded payload types and run
in a deterministic priority order. See :doc:`development` and :doc:`api` for
the detailed contracts.

Only one SQL backend is active when rasdaemon is running.

Event modules own their table descriptor and prepared statement, while the
database layer owns the active session. This separates decoding from backend
selection and permits a build without database support.
