/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2026 Mauro Carvalho Chehab <mchehab+huawei@kernel.org>
 */

#ifndef RAS_TEST_TRACE_MOCK_H
#define RAS_TEST_TRACE_MOCK_H

#include <stddef.h>
#include <stdio.h>

void trace_mock_start(void);
void trace_mock_stop(void);
void trace_mock_add_value(const char *name, unsigned long long value);
void trace_mock_add_raw(const char *name, const void *value, int length);
void trace_mock_set_filter_result(int result);
void system_mock_start(int result);
void system_mock_stop(void);
unsigned int system_mock_call_count(void);
const char *system_mock_last_command(void);
void popen_mock_start(const char *output, int status);
void popen_mock_stop(void);
unsigned int popen_mock_call_count(void);
void access_mock_start(const char *existing_path);
void access_mock_stop(void);
unsigned int access_mock_call_count(void);

#endif
