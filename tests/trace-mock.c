// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2026 Mauro Carvalho Chehab <mchehab+huawei@kernel.org>
 *
 * Scripted libtraceevent field access for event-handler unit tests.
 */

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <traceevent/event-parse.h>
#include <unistd.h>

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
static bool mock_popen;
static int mock_pclose_status;
static unsigned int mock_popen_calls;
static char mock_popen_output[1024];
static FILE *mock_popen_stream;
static bool mock_access;
static unsigned int mock_access_calls;
static char mock_access_path[PATH_MAX];

int __real_tep_get_field_val(struct trace_seq *s, struct tep_event *event,
			     const char *name, struct tep_record *record,
			     unsigned long long *value, int err);
void *__real_tep_get_field_raw(struct trace_seq *s, struct tep_event *event,
			      const char *name, struct tep_record *record,
			      int *length, int err);
enum tep_errno __real_tep_filter_match(struct tep_event_filter *filter,
				       struct tep_record *record);
int __real_system(const char *command);
FILE *__real_popen(const char *command, const char *type);
int __real_pclose(FILE *stream);
int __real_access(const char *pathname, int mode);

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

void popen_mock_start(const char *output, int status)
{
	assert(!mock_popen_stream);
	mock_popen = true;
	mock_pclose_status = status;
	mock_popen_calls = 0;
	if (output) {
		strncpy(mock_popen_output, output,
			sizeof(mock_popen_output) - 1);
		mock_popen_output[sizeof(mock_popen_output) - 1] = '\0';
	} else {
		mock_popen_output[0] = '\0';
	}
}

void popen_mock_stop(void)
{
	if (mock_popen_stream)
		fclose(mock_popen_stream);
	mock_popen_stream = NULL;
	mock_popen = false;
}

unsigned int popen_mock_call_count(void)
{
	return mock_popen_calls;
}

FILE *__wrap_popen(const char *command, const char *type)
{
	if (!mock_popen)
		return __real_popen(command, type);

	assert(!mock_popen_stream);
	assert(!strcmp(command, "ipmitool sel 2>&1"));
	assert(!strcmp(type, "r"));
	mock_popen_calls++;
	mock_popen_stream = tmpfile();
	if (!mock_popen_stream)
		return NULL;
	fputs(mock_popen_output, mock_popen_stream);
	rewind(mock_popen_stream);
	return mock_popen_stream;
}

int __wrap_pclose(FILE *stream)
{
	int status;

	if (!mock_popen || stream != mock_popen_stream)
		return __real_pclose(stream);

	status = mock_pclose_status;
	fclose(mock_popen_stream);
	mock_popen_stream = NULL;
	return status;
}

void access_mock_start(const char *existing_path)
{
	mock_access = true;
	mock_access_calls = 0;
	if (existing_path) {
		strncpy(mock_access_path, existing_path,
			sizeof(mock_access_path) - 1);
		mock_access_path[sizeof(mock_access_path) - 1] = '\0';
	} else {
		mock_access_path[0] = '\0';
	}
}

void access_mock_stop(void)
{
	mock_access = false;
}

unsigned int access_mock_call_count(void)
{
	return mock_access_calls;
}

int __wrap_access(const char *pathname, int mode)
{
	if (!mock_access)
		return __real_access(pathname, mode);

	mock_access_calls++;
	assert(mode == F_OK);
	if (mock_access_path[0] && !strcmp(pathname, mock_access_path))
		return 0;

	errno = ENOENT;
	return -1;
}
