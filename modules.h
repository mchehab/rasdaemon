// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2026 Mauro Carvalho Chehab <mchehab+huawei@kernel.org>
 */

#ifndef RAS_MODULE_H
#define RAS_MODULE_H

#include <stdbool.h>

struct ras_events;

/* Module entry: one per module, chained via ->next */
struct ras_module_entry {
	const char *name;

	int (*init)(struct ras_events *ras);
	void (*cleanup)(void);

	bool is_enabled;

	struct ras_module_entry *next;
};

/*
 * Priority list - linked list of modules for a given priority level.
 * head = NULL means the list is empty.
 */
struct module_list {
	struct ras_module_entry *head;
	struct module_list      *next;
};

/*
 * Register one module into the given priority list.
 * Returns 0 on success, non-zero if registration failed.
 */
int modules_register_core(struct ras_module_entry *entry,
			  struct ras_events *ras);
int modules_register_database(struct ras_module_entry *entry,
			      struct ras_events *ras);
int modules_register_event_handler(struct ras_module_entry *entry,
				   struct ras_events *ras);

/*
 * Unregister all modules
 */
void modules_unregister(void);


/*
 * Check whether any SQL backend has been initialized at runtime.
 */
bool modules_have_sql_backend(void);

/*
 * Check whether a named event handler has been registered and enabled in the
 * event-handler list.
 */
bool modules_event_handler_is_enabled(const char *name);

#endif /* RAS_MODULE_H */
