// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2026 Mauro Carvalho Chehab <mchehab+huawei@kernel.org>
 */

#ifndef RAS_MODULE_H
#define RAS_MODULE_H

#include <stdbool.h>

struct ras_events;
struct ras_module_ctx;

enum init_level {
	DB_MODULE,
	BASE_EVENT_MODULE,
	SUB_EVENT_MODULE,
	ACTIONS_MODULE,

	/* Should be the last one */
	MAX_LEVELS
};

struct ras_module_entry {
	const char *name;
	enum init_level level;

	int (*init)(struct ras_module_ctx *ctx);
	void (*cleanup)(struct ras_module_ctx *ctx);
};

struct ras_module_ctx {
	const struct ras_module_entry *entry;
	struct ras_events *ras;
	void *priv;
};


/*
 * Register one module into the given priority list.
 * Returns 0 on success, non-zero if registration failed.
 */
int module_register(const struct ras_module_entry *entry);

#define REGISTER_RAS_MODULE(entry) \
	static void __attribute__((constructor)) register_##entry(void) \
	{ \
		module_register(&(entry)); \
	}

int module_init(struct ras_events *ras, const char *name);
int module_cleanup(const char *name);

int modules_init(struct ras_events *ras);
void modules_cleanup_type(enum init_level level);
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

/* Check whether a named module was registered. */
bool module_is_registered(const char *name);

#ifdef HAVE_UNITTEST
enum test_group {
	TEST_GROUP_CORE,
	TEST_GROUP_EVENTS,
	TEST_GROUP_X86_EVENTS,
	TEST_GROUP_ARM_EVENTS,
	TEST_GROUP_RISCV_EVENTS,
	TEST_GROUP_ACTIONS,
	TEST_GROUP_DATABASE,
	TEST_GROUP_DB_SQLITE3,
	TEST_GROUP_DB_MYSQL,
	TEST_GROUP_DB_POSTGRESQL,
	TEST_GROUP_MODULES,

	TEST_GROUP_MAX
};

int module_test_register(enum test_group group, int (*run)(void),
			 unsigned int priority);
bool module_test_group_is_registered(enum test_group group);
int module_test_group_run(enum test_group group);
struct ras_module_ctx *module_test_context(const char *name);
#endif

#endif /* RAS_MODULE_H */
