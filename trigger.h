/* SPDX-License-Identifier: GPL-2.0 */

#ifndef __TRIGGER_H__
#define __TRIGGER_H__

#include "ras-record.h"

enum page_offline_trigger_type {
	PRE,
	POST,
};

struct event_trigger {
	const char *event_name;
	const char *env;
	char *path;
	char abs_path[256];
	int timeout;
};

int trigger_check(struct event_trigger *t);
void run_trigger(struct event_trigger *t, char *argv[], char **envr);
void setup_event_trigger(const char *event);

void run_mc_event_trigger(struct ras_mc_event *e);
void run_mce_record_trigger(struct mce_event *e);
void run_mf_event_trigger(struct ras_mf_event *e);
void run_aer_event_trigger(struct ras_aer_event *e);
void run_page_offline_trigger(unsigned long long addr, int otype, int type);

#endif
