// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2026 Mauro Carvalho Chehab <mchehab+huawei@kernel.org>
 */

#include <stdlib.h>
#include <string.h>
#include <sys/queue.h>

#include "core/modules.h"
#include "core/ras-logger.h"

struct ras_module_entry_runtime {
	const struct ras_module_entry *e;
	bool is_enabled;
	void *priv;

	LIST_ENTRY(ras_module_entry_runtime) node;
};

LIST_HEAD(module_list, ras_module_entry_runtime);

static struct module_list ras_modules = LIST_HEAD_INITIALIZER(ras_modules);

#ifdef HAVE_UNITTEST
struct module_test_runtime {
	enum test_group group;
	int (*run)(void);
	unsigned int priority;

	LIST_ENTRY(module_test_runtime) node;
};

LIST_HEAD(module_test_list, module_test_runtime);

static struct module_test_list module_tests =
	LIST_HEAD_INITIALIZER(module_tests);

static void module_tests_unregister(void)
{
	struct module_test_runtime *test;

	while ((test = LIST_FIRST(&module_tests))) {
		LIST_REMOVE(test, node);
		free(test);
	}
}
#endif

/*
 * Public functions
 */

int module_register(const struct ras_module_entry *entry)
{
	struct ras_module_entry_runtime *new, *cur, *prev = NULL;
	int cmp;

	if (!entry || !entry->name) {
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
	LIST_FOREACH(cur, &ras_modules, node) {
		cmp = strcmp(entry->name, cur->e->name);
		if (!cmp) {
			log(ALL, LOG_ERR, "module %s is already registered\n",
			    entry->name);
			free(new);
			return -EEXIST;
		}

		if (cmp < 0)
			break;

		prev = cur;
	}

	if (cur)
		LIST_INSERT_BEFORE(cur, new, node);
	else if (prev)
		LIST_INSERT_AFTER(prev, new, node);
	else
		LIST_INSERT_HEAD(&ras_modules, new, node);

	return 0;
}

bool modules_have_sql_backend(void)
{
	struct ras_module_entry_runtime *entry;

	LIST_FOREACH(entry, &ras_modules, node) {
		if (entry->e->level == DB_MODULE && entry->is_enabled)
			return true;
	}

	return false;
}

int module_init(struct ras_events *ras, const char *name)
{
	struct ras_module_entry_runtime *entry;

	LIST_FOREACH(entry, &ras_modules, node) {
		if (entry->e->postpone_init)
			continue;

		if (!strcmp(name, entry->e->name) && entry->e->init) {
			if (entry->e->init(name, ras, &entry->priv)) {
				log(ALL, LOG_ERR,
					"module %s init failed\n",
					entry->e->name);
			} else {
				entry->is_enabled = true;

				return false;
			}
		}
	}

	return true;
}

int module_cleanup(const char *name)
{
	struct ras_module_entry_runtime *entry;

	LIST_FOREACH(entry, &ras_modules, node) {
		if (!strcmp(name, entry->e->name) && entry->e->cleanup &&
		    entry->is_enabled) {
			entry->e->cleanup(entry->e, entry->priv);
			entry->is_enabled = false;

			return false;
		}
	}

	return true;
}

void modules_init(struct ras_events *ras)
{
	struct ras_module_entry_runtime *entry;

	for (int level = 0; level < MAX_LEVELS; level++) {
		LIST_FOREACH(entry, &ras_modules, node) {
			if (level == entry->e->level && entry->e->init) {
				if (entry->e->init(entry->e->name,
						   ras, &entry->priv)) {
					log(ALL, LOG_ERR,
					    "module %s init failed\n",
					    entry->e->name);
				} else {
					entry->is_enabled = true;
				}
			}
		}
	}
}

bool module_is_enabled(const char *name)
{
	struct ras_module_entry_runtime *entry;

	LIST_FOREACH(entry, &ras_modules, node) {
		if (!strcmp(entry->e->name, name))
			return entry->is_enabled;
	}

	return false;
}

bool module_is_registered(const char *name)
{
	struct ras_module_entry_runtime *entry;

	if (!name)
		return false;

	LIST_FOREACH(entry, &ras_modules, node)
		if (!strcmp(entry->e->name, name))
			return true;

	return false;
}

void modules_unregister(void)
{
	struct ras_module_entry_runtime *entry;

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
		LIST_FOREACH(entry, &ras_modules, node) {
			if (entry->e->level == level && entry->e->cleanup && entry->is_enabled)
				entry->e->cleanup(entry->e, entry->priv);
		}
	}

	/* Free them and remove head */
	while ((entry = LIST_FIRST(&ras_modules))) {
		LIST_REMOVE(entry, node);
		free(entry);
	}
}

#ifdef HAVE_UNITTEST
int module_test_register(enum test_group group, int (*run)(void),
			 unsigned int priority)
{
	struct module_test_runtime *test, *new, *prev = NULL;
	static bool cleanup_registered;

	if (group < 0 || group >= TEST_GROUP_MAX || !run)
		return -EINVAL;

	if (!cleanup_registered) {
		if (atexit(module_tests_unregister))
			return -ENOMEM;
		cleanup_registered = true;
	}

	LIST_FOREACH(test, &module_tests, node) {
		if (test->run == run)
			return -EEXIST;
	}

	new = malloc(sizeof(*new));
	if (!new)
		return -ENOMEM;

	new->group = group;
	new->run = run;
	new->priority = priority;

	LIST_FOREACH(test, &module_tests, node) {
		if (priority < test->priority) {
			LIST_INSERT_BEFORE(test, new, node);
			return 0;
		}

		prev = test;
	}

	if (prev)
		LIST_INSERT_AFTER(prev, new, node);
	else
		LIST_INSERT_HEAD(&module_tests, new, node);

	return 0;
}

bool module_test_group_is_registered(enum test_group group)
{
	struct module_test_runtime *test;

	LIST_FOREACH(test, &module_tests, node) {
		if (test->group == group)
			return true;
	}

	return false;
}

int module_test_group_run(enum test_group group)
{
	struct module_test_runtime *test;
	int failed = 0;

	LIST_FOREACH(test, &module_tests, node) {
		if (test->group == group && test->run())
			failed++;
	}

	return failed;
}
#endif
