// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2026 Mauro Carvalho Chehab <mchehab+huawei@kernel.org>
 *
 * Unit tests for dependency-free core helpers.
 */

#include "config.h"

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "core/bitfield.h"
#include "core/queue.h"
#include "core/ras-env.h"
#include "core/ras-events.h"
#include "core/ras-logger.h"
#include "core/rbtree.h"
#include "core/trigger.h"
#include "core/types.h"
#include "events-arch-x86/ras-mce-handler.h"
#include "tests/unittest.h"

#if defined(HAVE_MEMORY_CE_PFA) || defined(HAVE_MEMORY_ROW_CE_PFA)
struct test_tree_node {
	int key;
	struct rb_node node;
};

static void tree_insert(struct rb_root *root, struct test_tree_node *item)
{
	struct rb_node **link = &root->rb_node;
	struct rb_node *parent = NULL;

	while (*link) {
		struct test_tree_node *cur = rb_entry(*link,
						      struct test_tree_node, node);

		parent = *link;
		if (item->key < cur->key)
			link = &(*link)->rb_left;
		else
			link = &(*link)->rb_right;
	}

	rb_link_node(&item->node, parent, link);
	rb_insert_color(&item->node, root);
}
#endif

static void test_string_helpers(void **state)
{
	char buf[8];

	assert_int_equal(strscpy(buf, "ras", sizeof(buf)), 3);
	assert_string_equal(buf, "ras");
	assert_int_equal(strscat(buf, "-db", sizeof(buf)), 6);
	assert_string_equal(buf, "ras-db");
	assert_int_equal(strscpy(buf, "too-long-value", sizeof(buf)),
			 (size_t)-E2BIG);
	assert_string_equal(buf, "too-lon");
	assert_int_equal(GENMASK(7, 4), 0xf0);
	assert_int_equal(EXTRACT(0xabcd, 4, 7), 0xc);
}

static void test_bitfield_message(void **state)
{
	static const char * const names[] = { "zero", NULL, "two" };
	char buf[64];

	assert_int_equal(bitfield_msg(buf, sizeof(buf), names,
				      ARRAY_SIZE(names), 1, 0,
				      BIT_ULL(1) | BIT_ULL(2) | BIT_ULL(3)),
			 15);
	assert_string_equal(buf, "zero, BIT2, two");

	assert_int_equal(bitfield_msg(buf, 7, names, ARRAY_SIZE(names),
				      1, 0, BIT_ULL(1) | BIT_ULL(3)), 4);
	assert_string_equal(buf, "zero");
	assert_int_equal(bitfield_msg(NULL, 1, names, ARRAY_SIZE(names),
				      0, 0, 1), 0);
	assert_int_equal(bitfield_msg(buf, 0, names, ARRAY_SIZE(names),
				      0, 0, 1), 0);
}

static void test_bitfield_decoders(void **state)
{
	char *values[] = { NULL, "one", "two", NULL };
	struct field fields[] = {
		{ .start_bit = 2, .str = values,
		  .stringlen = ARRAY_SIZE(values) },
		{ .str = NULL },
	};
	struct numfield numbers[] = {
		{ .start = 0, .end = 3, .name = "count" },
		{ .start = 4, .end = 7, .name = "forced", .force = 1 },
		{ .name = NULL },
	};
	struct mce_event event = { 0 };

	decode_bitfield(&event, 2ULL << 2, fields);
	assert_string_equal(event.error_msg, "two");

	memset(event.error_msg, 0, sizeof(event.error_msg));
	decode_bitfield(&event, 3ULL << 2, fields);
	assert_string_equal(event.error_msg, "<2:3>");

	memset(event.error_msg, 0, sizeof(event.error_msg));
	decode_numfield(&event, 5, numbers);
	assert_non_null(strstr(event.error_msg, "count: 5"));
	assert_non_null(strstr(event.error_msg, "forced: 0"));
}

#ifdef HAVE_CPU_FAULT_ISOLATION
static void test_queue_lifecycle(void **state)
{
	struct link_queue *queue = init_queue();
	struct queue_node *first = node_create(10, 1);
	struct queue_node *second = node_create(20, 2);

	assert_non_null(queue);
	assert_true(is_empty(queue));
	assert_true(is_empty(NULL));
	assert_non_null(first);
	assert_non_null(second);
	push(queue, first);
	push(queue, second);
	assert_int_equal(queue->size, 2);
	assert_ptr_equal(front(queue), first);
	assert_int_equal(pop(queue), 0);
	assert_ptr_equal(front(queue), second);
	assert_int_equal(pop(queue), 0);
	assert_true(is_empty(queue));
	assert_null(queue->head);
	assert_null(queue->tail);
	assert_int_equal(pop(queue), -1);
	clear_queue(queue);
	free_queue(queue);
	free_queue(NULL);
}
#endif

#if defined(HAVE_MEMORY_CE_PFA) || defined(HAVE_MEMORY_ROW_CE_PFA)
static void test_rbtree_order_and_erase(void **state)
{
	static const int keys[] = { 4, 2, 6, 1, 3, 5, 7 };
	struct test_tree_node nodes[ARRAY_SIZE(keys)] = { 0 };
	struct rb_root root = { 0 };
	struct rb_node *node;
	int expected = 1;

	assert_true(RB_EMPTY_ROOT(&root));
	for (size_t i = 0; i < ARRAY_SIZE(keys); i++) {
		nodes[i].key = keys[i];
		tree_insert(&root, &nodes[i]);
	}

	for (node = rb_first(&root); node; node = rb_next(node))
		assert_int_equal(rb_entry(node, struct test_tree_node, node)->key,
				 expected++);
	assert_int_equal(expected, 8);

	expected = 7;
	for (node = rb_last(&root); node; node = rb_prev(node))
		assert_int_equal(rb_entry(node, struct test_tree_node, node)->key,
				 expected--);

	rb_erase(&nodes[0].node, &root);
	assert_int_equal(rb_entry(rb_first(&root), struct test_tree_node,
				  node)->key, 1);
	assert_int_equal(rb_entry(rb_last(&root), struct test_tree_node,
				  node)->key, 7);
}
#endif

static void test_environment_file(void **state)
{
	char path[] = "/tmp/rasdaemon-env-XXXXXX";
	static const char contents[] =
		"# comment\n"
		" RAS_TEST_ALPHA = one \n"
		"RAS_TEST_QUOTED=\"two words\"\n"
		"RAS_TEST_KEEP=file\n";
	int fd = mkstemp(path);

	assert_true(fd >= 0);
	assert_int_equal(write(fd, contents, sizeof(contents) - 1),
			 sizeof(contents) - 1);
	assert_int_equal(close(fd), 0);
	assert_int_equal(setenv("RAS_TEST_KEEP", "environment", 1), 0);
	assert_int_equal(ras_set_env(path), 0);
	assert_string_equal(getenv("RAS_TEST_ALPHA"), "one");
	assert_string_equal(getenv("RAS_TEST_QUOTED"), "two words");
	assert_string_equal(getenv("RAS_TEST_KEEP"), "environment");
	assert_int_equal(unlink(path), 0);
	assert_int_equal(ras_set_env(path), -1);
	unsetenv("RAS_TEST_ALPHA");
	unsetenv("RAS_TEST_QUOTED");
	unsetenv("RAS_TEST_KEEP");
}

static void test_trigger_validation(void **state)
{
	char path[] = "/tmp/rasdaemon-trigger-XXXXXX";
	int fd = mkstemp(path);

	assert_true(fd >= 0);
	assert_int_equal(fchmod(fd, 0700), 0);
	assert_int_equal(close(fd), 0);
	assert_string_equal(trigger_check(path), path);
	assert_int_equal(chmod(path, 0600), 0);
	assert_null(trigger_check(path));
	assert_int_equal(unlink(path), 0);
	assert_null(trigger_check(path));
}

static void test_mock_logger(void **state)
{
	ras_logger_clean();
	mock_output = true;
	log(TERM, LOG_INFO, "value=%d", 7);
	assert_non_null(mock_log_buf);
	assert_string_equal(mock_log_buf, "\tvalue=7");
	ras_logger_clean();
	assert_null(mock_log_buf);
	assert_int_equal(mock_log_len, 0);
}

static void test_warn_once(void **state)
{
	int i;

	ras_logger_clean();
	mock_output = true;
	for (i = 0; i < 3; i++)
		assert_int_equal(WARN_ONCE(i < 2, TERM, LOG_WARNING,
					   "warning %d", i), i < 2);
	assert_non_null(mock_log_buf);
	assert_string_equal(mock_log_buf, "\twarning 0");
	ras_logger_clean();
}

static void test_disabled_event_selection(void **state)
{
	char *saved = choices_disable;

	choices_disable = "ras:mc_event, mce:mce_record signal:signal_generate";
	assert_true(ras_events_test_is_disabled("ras", "mc_event"));
	assert_true(ras_events_test_is_disabled("mce", "mce_record"));
	assert_true(ras_events_test_is_disabled("signal", "signal_generate"));
	assert_false(ras_events_test_is_disabled("ras", "aer_event"));
	choices_disable = "ras:mc_event_extra";
	assert_false(ras_events_test_is_disabled("ras", "mc_event"));
	choices_disable = saved;
}

static const struct CMUnitTest tests[] = {
	cmocka_unit_test(test_string_helpers),
	cmocka_unit_test(test_bitfield_message),
	cmocka_unit_test(test_bitfield_decoders),
#ifdef HAVE_CPU_FAULT_ISOLATION
	cmocka_unit_test(test_queue_lifecycle),
#endif
#if defined(HAVE_MEMORY_CE_PFA) || defined(HAVE_MEMORY_ROW_CE_PFA)
	cmocka_unit_test(test_rbtree_order_and_erase),
#endif
	cmocka_unit_test(test_environment_file),
	cmocka_unit_test(test_trigger_validation),
	cmocka_unit_test(test_mock_logger),
	cmocka_unit_test(test_warn_once),
	cmocka_unit_test(test_disabled_event_selection),
};

int test_core(void)
{
	return _cmocka_run_group_tests("core helpers", tests,
				       ARRAY_SIZE(tests), NULL, NULL);
}

REGISTER_TEST(TEST_GROUP_CORE, test_core, 0);
