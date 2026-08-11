// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2026 Mauro Carvalho Chehab <mchehab+huawei@kernel.org>
 */

#ifndef RAS_MODULE_H
#define RAS_MODULE_H

#include <stdbool.h>

struct ras_events;

enum init_level {
	CORE_MODULE,
	DB_MODULE,
	BASE_EVENT_MODULE,
	SUB_EVENT_MODULE,

	/* Should be the last one */
	MAX_LEVELS
};

/* Module entry: one per module, chained via ->next */
struct ras_module_entry {
	const char *name;

	const int (*init)(const char *name, struct ras_events *ras, void **priv);
	const void (*cleanup)(const struct ras_module_entry *entry, void *priv);

	const enum init_level level;
};


struct ras_module_entry_runtime {
	const struct ras_module_entry *e;

	bool is_enabled;
	bool missing_deps;

	void *priv;

	struct ras_module_entry_runtime *next;
};

/*
 * Priority list - linked list of modules for a given priority level.
 * head = NULL means the list is empty.
 */
struct module_list {
	struct ras_module_entry_runtime *head;
	struct module_list     		*next;
};

/*
 * Register one module into the given priority list.
 * Returns 0 on success, non-zero if registration failed.
 */
int module_register(const struct ras_module_entry *entry);

int module_init(struct ras_events *ras, const char *name);

void modules_init(struct ras_events *ras);
void modules_unregister(void);

/*
 * Check whether any SQL backend has been initialized at runtime.
 */
bool modules_have_sql_backend(void);

/*
 * Check whether a named event handler has been registered and enabled in the
 * event-handler list.
 */
bool module_is_enabled(const char *name);

#endif /* RAS_MODULE_H */
