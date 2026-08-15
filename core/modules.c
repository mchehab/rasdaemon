// SPDX-License-Identifier: GPL-2.0-or-later
/*
* Copyright (C) 2026 Mauro Carvalho Chehab <mchehab+huawei@kernel.org>
*/

#include <stdlib.h>
#include <string.h>

#include "core/modules.h"
#include "core/ras-logger.h"

/* Let's not declare it as static, as we want to access it on unit tests */
struct module_list ras_modules = { 0 };

/*
 * Public functions
 */

int module_register(const struct ras_module_entry *entry)
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
		new->next =*head;
		*head = new;
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

int module_init(struct ras_events *ras, const char *name)
{
	struct ras_module_entry_runtime *entry;

	for (entry = ras_modules.head; entry; entry = entry->next) {
		if (!strcmp(name, entry->e->name) && entry->e->init) {
			if (entry->e->init(entry->e->name, ras, &entry->priv)) {
				log(ALL, LOG_ERR,
					"module %s init failed\n",
					entry->e->name);
			} else {
				log(ALL, LOG_INFO,
					"module %s enabled\n",
					entry->e->name);
				entry->is_enabled = true;

				return false;
			}
		}
	}

	return true;
}

void modules_init(struct ras_events *ras)
{
	struct ras_module_entry_runtime *entry;

	for (int level = 0; level < MAX_LEVELS; level++) {
		for (entry = ras_modules.head; entry; entry = entry->next) {
			if (level == entry->e->level && entry->e->init) {
				if (entry->e->init(entry->e->name,
						   ras, &entry->priv)) {
					log(ALL, LOG_ERR,
					    "module %s init failed\n",
					    entry->e->name);
				} else {
					log(ALL, LOG_INFO,
					    "module %s enabled\n",
					    entry->e->name);
					entry->is_enabled = true;
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
	struct ras_module_entry_runtime *entry, *tmp;

	/*
	 * Cleanup each module
	 *
	 * NOTE:
	 *	The logic here assumes that modules at the same level
	 *	can be unregistered in any order. It means that modules
	 *	that has HAVE_ dependencies to other modules at event_list
	 *	needs to handle cleanup() even if called after other event
	 *	list requrements.
	 */
	for (int level = MAX_LEVELS - 1; level >= 0; level--) {
		entry  = ras_modules.head;

		while (entry) {
			tmp = entry->next;
			if (entry->e->level == level && entry->e->cleanup && entry->is_enabled)
				entry->e->cleanup(entry->e, entry->priv);

			entry = entry->next;
		}
	}

	/* Free them and remove head */
	entry = ras_modules.head;
	while (entry) {
		tmp = entry->next;
		free(entry);
		entry = tmp;
	}

	ras_modules.head = NULL;
}
