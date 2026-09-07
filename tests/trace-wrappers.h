/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Linker wrapper declarations used by the trace-event test mocks.
 */

#ifndef RAS_TEST_TRACE_WRAPPERS_H
#define RAS_TEST_TRACE_WRAPPERS_H

#include <stdio.h>
#include <traceevent/event-parse.h>

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

#endif
