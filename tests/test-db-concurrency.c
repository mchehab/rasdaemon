// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (C) 2026 Mauro Carvalho Chehab <mchehab+huawei@kernel.org> */

#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

#include "db/ras-db.h"
#include "tests/test-db-concurrency.h"

#define WRITES_PER_THREAD 64

static const struct db_fields concurrency_fields[] = {
	{ .name = "id", .type = DB_TYPE_INT32, .is_pk = true },
	{ .name = "writer", .type = DB_TYPE_INT32 },
};

static const struct db_table_descriptor concurrency_table = {
	.name = "concurrency_test",
	.fields = concurrency_fields,
	.num_fields = ARRAY_SIZE(concurrency_fields),
};

struct concurrency_test {
	struct ras_events *ras;
	struct ras_stmt *stmt;
	atomic_bool start;
	atomic_int failures;
	atomic_int contentions;
};

struct writer_test {
	struct concurrency_test *test;
	int writer;
	int cpu;
};

static int *allowed_cpus(int *count)
{
	cpu_set_t allowed;
	int *cpus;
	int cpu, index = 0;

	if (sched_getaffinity(0, sizeof(allowed), &allowed))
		return NULL;
	*count = CPU_COUNT(&allowed);
	if (!*count)
		return NULL;
	cpus = calloc(*count, sizeof(*cpus));
	if (!cpus)
		return NULL;

	for (cpu = 0; cpu < CPU_SETSIZE; cpu++) {
		if (CPU_ISSET(cpu, &allowed))
			cpus[index++] = cpu;
	}

	return cpus;
}

static void *concurrent_writer(void *data)
{
	struct writer_test *writer = data;
	struct concurrency_test *test = writer->test;
	cpu_set_t affinity;
	int iteration, lock_rc, rc;

	CPU_ZERO(&affinity);
	CPU_SET(writer->cpu, &affinity);
	if (pthread_setaffinity_np(pthread_self(), sizeof(affinity), &affinity)) {
		atomic_fetch_add(&test->failures, 1);
		return NULL;
	}

	while (!atomic_load(&test->start))
		sched_yield();

	for (iteration = 0; iteration < WRITES_PER_THREAD; iteration++) {
		lock_rc = pthread_mutex_trylock(&test->ras->db_lock);
		if (lock_rc == EBUSY) {
			atomic_fetch_add(&test->contentions, 1);
			lock_rc = pthread_mutex_lock(&test->ras->db_lock);
		}
		if (lock_rc) {
			atomic_fetch_add(&test->failures, 1);
			continue;
		}
		if (!iteration)
			usleep(1000);

		rc = db_bind(&concurrency_table, test->stmt, 1,
			     writer->writer * WRITES_PER_THREAD + iteration + 1, -1);
		if (!rc)
			rc = db_bind(&concurrency_table, test->stmt, 2,
				     writer->writer, -1);
		if (!rc)
			rc = db_eval_stmt(test->stmt, concurrency_table.name);
		if (rc)
			atomic_fetch_add(&test->failures, 1);

		pthread_mutex_unlock(&test->ras->db_lock);
	}

	return NULL;
}

int test_db_concurrent_writers(struct ras_events *ras,
			       db_concurrency_count_fn count_rows)
{
	struct concurrency_test test = { .ras = ras };
	struct writer_test *writers;
	pthread_t *threads;
	int *cpus, cpu_count, created = 0;
	int expected, index, rc = 0;

	cpus = allowed_cpus(&cpu_count);
	if (!cpus)
		return -1;
	if (cpu_count < 2) {
		free(cpus);
		return -EAGAIN;
	}
	writers = calloc(cpu_count, sizeof(*writers));
	threads = calloc(cpu_count, sizeof(*threads));
	if (!writers || !threads) {
		free(cpus);
		free(writers);
		free(threads);
		return -ENOMEM;
	}

	if (db_exec_sql(ras->db, "DROP TABLE IF EXISTS concurrency_test") ||
	    db_create_table(ras->db, &concurrency_table) ||
	    db_prepare_insert_stmt(ras->db, &test.stmt, &concurrency_table)) {
		free(cpus);
		free(writers);
		free(threads);
		return -1;
	}

	pthread_mutex_init(&ras->db_lock, NULL);
	for (index = 0; index < cpu_count; index++) {
		writers[index].test = &test;
		writers[index].writer = index;
		writers[index].cpu = cpus[index];
		rc = pthread_create(&threads[index], NULL, concurrent_writer,
				    &writers[index]);
		if (rc)
			break;
		created++;
	}

	if (created != cpu_count)
		atomic_fetch_add(&test.failures, 1);
	atomic_store(&test.start, true);

	for (index = 0; index < created; index++)
		pthread_join(threads[index], NULL);

	expected = cpu_count * WRITES_PER_THREAD;
	if (atomic_load(&test.failures) || !atomic_load(&test.contentions) ||
	    count_rows(ras->db, concurrency_table.name) != expected)
		rc = -1;

	pthread_mutex_destroy(&ras->db_lock);
	db_finalize(test.stmt);
	free(cpus);
	free(writers);
	free(threads);

	return rc;
}
