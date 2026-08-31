// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2026 Mauro Carvalho Chehab <mchehab+huawei@kernel.org>
 */

#ifndef RAS_MODULE_H
#define RAS_MODULE_H

#include <stdbool.h>

struct ras_events;
struct ras_module_ctx;

/**
 * enum init_level - module initialization and cleanup order
 * @DB_MODULE: database backends
 * @BASE_EVENT_MODULE: base event decoders and table owners
 * @SUB_EVENT_MODULE: decoders which depend on base event modules
 * @ACTIONS_MODULE: consumers of decoded events
 * @ACTIONS_SUB_MODULE: consumer modules that depend on a base consumer
 * @MAX_LEVELS: number of initialization levels
 *
 * Initialization proceeds from @DB_MODULE to @ACTIONS_MODULE. Process-wide
 * cleanup visits the levels in reverse order.
 */
enum init_level {
	DB_MODULE,
	BASE_EVENT_MODULE,
	SUB_EVENT_MODULE,
	ACTIONS_MODULE,
	ACTIONS_SUB_MODULE,

	/* Should be the last one */
	MAX_LEVELS
};

/**
 * struct ras_module_entry - immutable module registration descriptor
 * @name: unique module name
 * @level: initialization level
 * @init: optional initialization callback
 * @cleanup: optional cleanup callback
 *
 * The descriptor must have static lifetime. On a successful @init, @cleanup
 * receives the same context and must release all module-owned resources.
 */
struct ras_module_entry {
	const char *name;
	enum init_level level;

	int (*init)(struct ras_module_ctx *ctx);
	void (*cleanup)(struct ras_module_ctx *ctx);
};

/**
 * struct ras_module_ctx - runtime context owned by the module registry
 * @entry: static module descriptor
 * @ras: event-loop context supplied during initialization
 * @priv: module-private state managed by the callbacks
 */
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
/**
 * enum test_group - independently selectable unit-test families
 * @TEST_GROUP_CORE: generic core tests
 * @TEST_GROUP_EVENTS: architecture-independent event tests
 * @TEST_GROUP_X86_EVENTS: x86 event tests
 * @TEST_GROUP_ARM_EVENTS: Arm event tests
 * @TEST_GROUP_RISCV_EVENTS: RISC-V event tests
 * @TEST_GROUP_ACTIONS: event-consumer tests
 * @TEST_GROUP_DATABASE: generic database tests
 * @TEST_GROUP_DB_SQLITE3: SQLite tests
 * @TEST_GROUP_DB_MYSQL: MySQL/MariaDB tests
 * @TEST_GROUP_DB_POSTGRESQL: PostgreSQL tests
 * @TEST_GROUP_MODULES: module-registry tests
 * @TEST_GROUP_MAX: number of test groups
 */
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
