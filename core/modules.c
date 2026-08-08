// SPDX-License-Identifier: GPL-2.0-or-later
/*
* Copyright (C) 2026 Mauro Carvalho Chehab <mchehab+huawei@kernel.org>
*/

#include <stdlib.h>
#include <string.h>

#include "core/modules.h"
#include "core/ras-logger.h"

static struct module_list core_list   = { NULL, NULL };
static struct module_list db_list     = { NULL, NULL };
static struct module_list event_list  = { NULL, NULL };

/*
 * Helpers to handle individual module add/remove and check if enabled
 */

static int module_add(struct ras_module_entry **head,
		      struct ras_module_entry *entry,
		      struct ras_events *ras)
{
	int ret = 0;

	if (!entry)
		return -EINVAL;

	entry->next = *head;
	*head       = entry;

	/* Module-specific handler */

	entry->is_enabled = false;

	if (entry->init) {
		int rc = entry->init(ras);

		if (rc) {
			log(ALL, LOG_ERR, "core module %s init failed\n", entry->name);
			ret = 1;
		} else {
			entry->is_enabled = true;
		}
	}

	return ret;

}

static void module_remove(struct module_list *list)
{
	struct ras_module_entry *cur  = list->head;

	while (cur) {
		struct ras_module_entry *tmp = cur->next;

		if (cur->cleanup)
			cur->cleanup();

		free(cur);
		cur = tmp;
	}
}

static bool module_is_enabled(struct module_list *list, const char *name)
{
	struct ras_module_entry *entry = list->head;

	while (entry) {
		if (!strcmp(entry->name, name))
			return true;

		entry = entry->next;
	}
	return false;
}


/*
 * public API
 */

bool modules_have_sql_backend(void)
{
	struct ras_module_entry *entry = db_list.head;

	while (entry) {
		if (entry->is_enabled)
			return true;

		entry = entry->next;
	}

	return false;
}

bool modules_event_handler_is_enabled(const char *name)
{
	return module_is_enabled(&event_list, name);
}

int modules_register_core(struct ras_module_entry *entry,
			  struct ras_events *ras)
{
	return module_add(&core_list.head, entry, ras);
}

int modules_register_database(struct ras_module_entry *entry,
			      struct ras_events *ras)
{
	/* Only one DB backend can be used */
	if (modules_have_sql_backend()) {
		log(ALL, LOG_ERR,
		    "database %s already registered, refusing second one\n",
		    entry->name);

		return 1;
	}

	return module_add(&db_list.head, entry, ras);
}

int modules_register_event_handler(struct ras_module_entry *entry,
				   struct ras_events *ras)
{
	return module_add(&event_list.head, entry, ras);
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
	module_remove(&event_list);
	module_remove(&db_list);
	module_remove(&core_list);
}
