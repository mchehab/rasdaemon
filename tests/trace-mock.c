// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2026 Mauro Carvalho Chehab <mchehab+huawei@kernel.org>
 *
 * Scripted libtraceevent field access for event-handler unit tests.
 */

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <traceevent/event-parse.h>

#include "tests/trace-mock.h"

#define MAX_MOCK_FIELDS 96

struct mock_field {
	const char *name;
	unsigned long long value;
	const void *raw;
	int length;
	bool is_raw;
};

static struct mock_field fields[MAX_MOCK_FIELDS];
static size_t field_count;
static bool mock_active;
static int filter_result;
static bool mock_system;
static int mock_system_result;
static unsigned int mock_system_calls;
static char mock_system_command[512];

int __real_tep_get_field_val(struct trace_seq *s, struct tep_event *event,
			     const char *name, struct tep_record *record,
			     unsigned long long *value, int err);
void *__real_tep_get_field_raw(struct trace_seq *s, struct tep_event *event,
			      const char *name, struct tep_record *record,
			      int *length, int err);
enum tep_errno __real_tep_filter_match(struct tep_event_filter *filter,
				       struct tep_record *record);
int __real_system(const char *command);

void trace_mock_start(void)
{
	field_count = 0;
	filter_result = FILTER_NONE;
	mock_active = true;
}

void trace_mock_stop(void)
{
	mock_active = false;
	field_count = 0;
}

void trace_mock_add_value(const char *name, unsigned long long value)
{
	assert(field_count < MAX_MOCK_FIELDS);
	fields[field_count++] = (struct mock_field) {
		.name = name,
		.value = value,
	};
}

void trace_mock_add_raw(const char *name, const void *value, int length)
{
	assert(field_count < MAX_MOCK_FIELDS);
	fields[field_count++] = (struct mock_field) {
		.name = name,
		.raw = value,
		.length = length,
		.is_raw = true,
	};
}

void trace_mock_set_filter_result(int result)
{
	filter_result = result;
}

int __wrap_tep_get_field_val(struct trace_seq *s, struct tep_event *event,
			     const char *name, struct tep_record *record,
			     unsigned long long *value, int err)
{
	if (!mock_active)
		return __real_tep_get_field_val(s, event, name, record, value, err);

	for (size_t i = 0; i < field_count; i++) {
		if (!fields[i].is_raw && !strcmp(fields[i].name, name)) {
			*value = fields[i].value;
			return 0;
		}
	}

	return -1;
}

void *__wrap_tep_get_field_raw(struct trace_seq *s, struct tep_event *event,
			      const char *name, struct tep_record *record,
			      int *length, int err)
{
	if (!mock_active)
		return __real_tep_get_field_raw(s, event, name, record, length, err);

	for (size_t i = 0; i < field_count; i++) {
		if (fields[i].is_raw && !strcmp(fields[i].name, name)) {
			if (length)
				*length = fields[i].length;
			return (void *)fields[i].raw;
		}
	}

	return NULL;
}

enum tep_errno __wrap_tep_filter_match(struct tep_event_filter *filter,
				       struct tep_record *record)
{
	if (!mock_active)
		return __real_tep_filter_match(filter, record);

	return filter_result;
}

void system_mock_start(int result)
{
	mock_system = true;
	mock_system_result = result;
	mock_system_calls = 0;
	mock_system_command[0] = '\0';
}

void system_mock_stop(void)
{
	mock_system = false;
}

unsigned int system_mock_call_count(void)
{
	return mock_system_calls;
}

const char *system_mock_last_command(void)
{
	return mock_system_command;
}

int __wrap_system(const char *command)
{
	if (!mock_system)
		return __real_system(command);

	mock_system_calls++;
	if (command) {
		strncpy(mock_system_command, command,
			sizeof(mock_system_command) - 1);
		mock_system_command[sizeof(mock_system_command) - 1] = '\0';
	}
	return mock_system_result;
}
