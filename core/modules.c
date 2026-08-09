// SPDX-License-Identifier: GPL-2.0-or-later
/*
* Copyright (C) 2026 Mauro Carvalho Chehab <mchehab+huawei@kernel.org>
*/

#include <stdlib.h>
#include <string.h>

#include "core/modules.h"
#include "core/ras-logger.h"

static struct module_list ras_modules = { 0 };

/*
 * Public functions
 */

int module_register(struct ras_module_entry *entry)
{
	struct ras_module_entry_runtime **head = &ras_modules.head;
	struct ras_module_entry_runtime *new, *cur, *prev = NULL;

	if (!entry) {
		log(ALL, LOG_ERR, "module entry is missing!\n");
		return -EINVAL;
	}

	new = calloc(1, sizeof(*new));
	if (!new) {
		log(ALL, LOG_ERR, "no memory to register module %s\n",
		    entry->name);
		return -ENOMEM;
	}

	new->e = entry;

	/* Keep it alphabetically sorted */
	for (cur = ras_modules.head; cur; cur = cur->next) {
		if (strcmp(entry->name, cur->e->name) < 0)
		break;
		prev = cur;
	}

	if (!prev) {
		new->next = NULL;
		(*head) = new;
	} else {
		new->next = prev->next;
		prev->next = new;
	}

	return 0;
}

bool modules_have_sql_backend(void)
{
	struct ras_module_entry_runtime *entry = ras_modules.head;

	while (entry) {
		if (entry->e->level == DB_MODULE && entry->is_enabled)
			return true;

		entry = entry->next;
	}

	return false;
}

void modules_init(struct ras_events *ras)
{
	struct ras_module_entry_runtime *entry;
	bool enabled_db = false;

	for (int level = 0; level < MAX_LEVELS; level++) {
		for (entry = ras_modules.head; entry; entry = entry->next) {
			if (level == entry->e->level && entry->e->init) {
				/* Only one database can be enabled */
				if (level == DB_MODULE && enabled_db)
					continue;

				if (entry->e->init(ras)) {
					log(ALL, LOG_ERR,
					    "module %s init failed\n",
					    entry->e->name);
				} else {
					entry->is_enabled = true;
					if (level == DB_MODULE)
						enabled_db = true;
				}
			}
		}
	}
}

bool module_is_enabled(const char *name)
{
	struct ras_module_entry_runtime *entry = ras_modules.head;

	while (entry) {
		if (!strcmp(entry->e->name, name))
			return true;

		entry = entry->next;
	}

	return false;
}

void modules_unregister(void)
{
	/*
	 * NOTE:
	 *	The logic here assumes that modules at the same level
	 *	can be unregistered in any order. It means that modules
	 *	that has HAVE_ dependencies to other modules at event_list
	 *	needs to handle cleanup() even if called after other event
	 *	list requrements.
	 */

	for (int level = MAX_LEVELS - 1; level > 0; level--) {
		struct ras_module_entry_runtime *cur  = ras_modules.head;

		while (cur) {
			struct ras_module_entry_runtime *tmp = cur->next;

			if (cur->e->cleanup)
				cur->e->cleanup();

			free(cur);
			cur = tmp;
		}
	}
}
