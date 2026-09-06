/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef TEST_DB_CONCURRENCY_H
#define TEST_DB_CONCURRENCY_H

#include "core/ras-events.h"

typedef int (*db_concurrency_count_fn)(struct ras_db *db, const char *table);

int test_db_concurrent_writers(struct ras_events *ras,
			       db_concurrency_count_fn count_rows);

#endif
