.. SPDX-License-Identifier: GPL-2.0-only

RAS daemon internal API
=======================

Environment and common types
----------------------------

.. kernel-doc:: core/ras-env.c

.. kernel-doc:: core/types.h

.. kernel-doc:: core/types.c

Module and event registries
---------------------------

.. kernel-doc:: core/modules.h

.. kernel-doc:: core/modules.c

.. kernel-doc:: core/ras-events.h

.. kernel-doc:: core/event-consumer.c

.. kernel-doc:: core/ras-events.c

Core utilities
--------------

.. kernel-doc:: core/bitfield.h

.. kernel-doc:: core/bitfield.c

.. kernel-doc:: core/queue.h

.. kernel-doc:: core/queue.c

.. kernel-doc:: core/rbtree.h

.. kernel-doc:: core/rbtree.c

.. kernel-doc:: core/ras-logger.c

.. kernel-doc:: core/trigger.h

.. kernel-doc:: core/trigger.c

Process entry point
-------------------

.. kernel-doc:: core/rasdaemon.c

Database API
------------

.. kernel-doc:: db/ras-db.h

.. kernel-doc:: db/ras-db.c
   :identifiers: selected_backend ras_db_ops ras_db_backend_runtime \
                 ras_db_backends rasdaemon_hostname rasdaemon_hostname_buf \
                 add_hostname \
                 db_backend_register db_backend_unregister \
                 db_backend_is_registered db_get_rasdaemon_hostname \
                 db_bind_type

.. kernel-doc:: db/db-mysql.h

.. kernel-doc:: db/db-postgresql.h
