// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * Copyright (C) 2013 Mauro Carvalho Chehab <mchehab+huawei@kernel.org>
 */

#define _GNU_SOURCE
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <traceevent/kbuffer.h>
#include <unistd.h>

#include "ras-logger.h"
#include "ras-mc-handler.h"
#include "ras-page-isolation.h"
#include "ras-report.h"
#include "trigger.h"
#include "types.h"

#define MAX_ENV 30
static const char *mc_ce_trigger = NULL;
static const char *mc_ue_trigger = NULL;

void mc_event_trigger_setup(void)
{
	const char *trigger;

	trigger = getenv("MC_CE_TRIGGER");
	if (trigger && strcmp(trigger, "")) {
		mc_ce_trigger = trigger_check(trigger);

		if (!mc_ce_trigger) {
			log(ALL, LOG_ERR,
			    "Cannot access mc_event ce trigger `%s`\n",
			    trigger);
		} else {
			log(ALL, LOG_INFO,
			    "Setup mc_event ce trigger `%s`\n",
			    trigger);
		}
	}

	trigger = getenv("MC_UE_TRIGGER");
	if (trigger && strcmp(trigger, "")) {
		mc_ue_trigger = trigger_check(trigger);

		if (!mc_ue_trigger) {
			log(ALL, LOG_ERR,
			    "Cannot access mc_event ue trigger `%s`\n",
			    trigger);
		} else {
			log(ALL, LOG_INFO,
			    "Setup mc_event ue trigger `%s`\n",
			    trigger);
		}
	}
}

static void run_mc_trigger(struct ras_mc_event *ev, const char *mc_trigger)
{
	char *env[MAX_ENV];
	int ei = 0;
	int i;

	if (asprintf(&env[ei++], "PATH=%s", getenv("PATH") ?: "/sbin:/usr/sbin:/bin:/usr/bin") < 0)
		goto free;
	if (asprintf(&env[ei++], "TIMESTAMP=%s", ev->timestamp) < 0)
		goto free;
	if (asprintf(&env[ei++], "COUNT=%d", ev->error_count) < 0)
		goto free;
	if (asprintf(&env[ei++], "TYPE=%s", ev->error_type) < 0)
		goto free;
	if (asprintf(&env[ei++], "MESSAGE=%s", ev->msg) < 0)
		goto free;
	if (asprintf(&env[ei++], "LABEL=%s", ev->label) < 0)
		goto free;
	if (asprintf(&env[ei++], "MC_INDEX=%d", ev->mc_index) < 0)
		goto free;
	if (asprintf(&env[ei++], "TOP_LAYER=%d", ev->top_layer) < 0)
		goto free;
	if (asprintf(&env[ei++], "MIDDLE_LAYER=%d", ev->middle_layer) < 0)
		goto free;
	if (asprintf(&env[ei++], "LOWER_LAYER=%d", ev->lower_layer) < 0)
		goto free;
	if (asprintf(&env[ei++], "ADDRESS=%llx", ev->address) < 0)
		goto free;
	if (asprintf(&env[ei++], "GRAIN=%lld", ev->grain) < 0)
		goto free;
	if (asprintf(&env[ei++], "SYNDROME=%llx", ev->syndrome) < 0)
		goto free;
	if (asprintf(&env[ei++], "DRIVER_DETAIL=%s", ev->driver_detail) < 0)
		goto free;
	env[ei] = NULL;
	assert(ei < MAX_ENV);

	run_trigger(mc_trigger, NULL, env, "mc_event");

free:
	for (i = 0; i < ei; i++)
		free(env[i]);
}

int ras_mc_event_handler(struct trace_seq *s,
			 struct tep_record *record,
			 struct tep_event *event, void *context)
{
	int len;
	unsigned long long val;
	struct ras_events *ras = context;
	time_t now;
	struct tm *tm;
	struct ras_mc_event ev;
	int parsed_fields = 0;
	const char *level;

	if (tep_get_field_val(s, event, "error_type", record, &val, 1) < 0)
		goto parse_error;
	parsed_fields++;

	switch (val) {
	case HW_EVENT_ERR_CORRECTED:
		ev.error_type = "Corrected";
		break;
	case HW_EVENT_ERR_UNCORRECTED:
		ev.error_type = "Uncorrected";
		break;
	case HW_EVENT_ERR_DEFERRED:
		ev.error_type = "Deferred";
		break;
	case HW_EVENT_ERR_FATAL:
		ev.error_type = "Fatal";
		break;
	case HW_EVENT_ERR_INFO:
	default:
		ev.error_type = "Info";
	}

	switch (val) {
	case HW_EVENT_ERR_UNCORRECTED:
	case HW_EVENT_ERR_DEFERRED:
		level = loglevel_str[LOGLEVEL_CRIT];
		break;
	case HW_EVENT_ERR_FATAL:
		level = loglevel_str[LOGLEVEL_EMERG];
		break;
	case HW_EVENT_ERR_CORRECTED:
		level = loglevel_str[LOGLEVEL_ERR];
		break;
	default:
		level = loglevel_str[LOGLEVEL_DEBUG];
		break;
	}
	trace_seq_printf(s, "%s ", level);

	/*
	 * Newer kernels (3.10-rc1 or upper) provide an uptime clock.
	 * On previous kernels, the way to properly generate an event would
	 * be to inject a fake one, measure its timestamp and diff it against
	 * gettimeofday. We won't do it here. Instead, let's use uptime,
	 * falling-back to the event report's time, if "uptime" clock is
	 * not available (legacy kernels).
	 */

	if (ras->use_uptime)
		now = record->ts / user_hz + ras->uptime_diff;
	else
		now = time(NULL);

	tm = localtime(&now);
	if (tm)
		strftime(ev.timestamp, sizeof(ev.timestamp),
			 "%Y-%m-%d %H:%M:%S %z", tm);
	trace_seq_printf(s, "%s ", ev.timestamp);

	if (tep_get_field_val(s,  event, "error_count", record, &val, 1) < 0)
		goto parse_error;
	parsed_fields++;

	ev.error_count = val;
	trace_seq_printf(s, "%d ", ev.error_count);

	trace_seq_puts(s, ev.error_type);
	if (ev.error_count > 1)
		trace_seq_puts(s, " errors:");
	else
		trace_seq_puts(s, " error:");

	ev.msg = tep_get_field_raw(s, event, "msg", record, &len, 1);
	if (!ev.msg)
		goto parse_error;
	parsed_fields++;

	if (*ev.msg) {
		trace_seq_puts(s, " ");
		trace_seq_puts(s, ev.msg);
	}

	ev.label = tep_get_field_raw(s, event, "label", record, &len, 1);
	if (!ev.label)
		goto parse_error;
	parsed_fields++;

	if (*ev.label) {
		trace_seq_puts(s, " on ");
		trace_seq_puts(s, ev.label);
	}

	trace_seq_puts(s, " (");
	if (tep_get_field_val(s,  event, "mc_index", record, &val, 1) < 0)
		goto parse_error;
	parsed_fields++;

	ev.mc_index = val;
	trace_seq_printf(s, "mc: %d", ev.mc_index);

	if (tep_get_field_val(s,  event, "top_layer", record, &val, 1) < 0)
		goto parse_error;
	parsed_fields++;
	ev.top_layer = (signed char)val;

	if (tep_get_field_val(s,  event, "middle_layer", record, &val, 1) < 0)
		goto parse_error;
	parsed_fields++;
	ev.middle_layer = (signed char)val;

	if (tep_get_field_val(s,  event, "lower_layer", record, &val, 1) < 0)
		goto parse_error;
	parsed_fields++;
	ev.lower_layer = (signed char)val;

	if (ev.top_layer >= 0 || ev.middle_layer >= 0 || ev.lower_layer >= 0) {
		if (ev.lower_layer >= 0)
			trace_seq_printf(s, " location: %d:%d:%d",
					 ev.top_layer, ev.middle_layer, ev.lower_layer);
		else if (ev.middle_layer >= 0)
			trace_seq_printf(s, " location: %d:%d",
					 ev.top_layer, ev.middle_layer);
		else
			trace_seq_printf(s, " location: %d", ev.top_layer);
	}

	if (tep_get_field_val(s,  event, "address", record, &val, 1) < 0)
		goto parse_error;
	parsed_fields++;

	ev.address = val;
	if (ev.address)
		trace_seq_printf(s, " address: 0x%08llx", ev.address);

	if (tep_get_field_val(s,  event, "grain_bits", record, &val, 1) < 0)
		goto parse_error;
	parsed_fields++;

	ev.grain = val;
	trace_seq_printf(s, " grain: %lld", ev.grain);

	if (tep_get_field_val(s,  event, "syndrome", record, &val, 1) < 0)
		goto parse_error;
	parsed_fields++;

	ev.syndrome = val;
	if (val)
		trace_seq_printf(s, " syndrome: 0x%08llx", ev.syndrome);

	ev.driver_detail = tep_get_field_raw(s, event, "driver_detail", record,
					     &len, 1);
	if (!ev.driver_detail)
		goto parse_error;
	parsed_fields++;

	if (*ev.driver_detail) {
		trace_seq_puts(s, " ");
		trace_seq_puts(s, ev.driver_detail);
	}
	trace_seq_puts(s, ")");

	/* Insert data into the SGBD */

	ras_store_mc_event(ras, &ev);

#ifdef HAVE_MEMORY_CE_PFA
	/* Account page corrected errors */
	if (!strcmp(ev.error_type, "Corrected"))
		ras_record_page_error(ev.address, ev.error_count, now);
#endif

#ifdef HAVE_MEMORY_ROW_CE_PFA
	/* Account row corrected errors */
	struct timespec ts;
	clockid_t clk_id = CLOCK_MONOTONIC;
	// A fault occurs, but the fault error_count BIOS reports sometimes is 0.
	// This is a bug in the BIOS.
	// We set the value to 1
	// even if the error_count is reported 0.
	if (ev.error_count == 0)
		ev.error_count = 1;
	if (clock_gettime(clk_id, &ts) == 0 && !strcmp(ev.error_type, "Corrected"))
		ras_record_row_error(ev.driver_detail, ev.error_count,
				     ts.tv_sec, ev.address);
#endif

#ifdef HAVE_ABRT_REPORT
	/* Report event to ABRT */
	ras_report_mc_event(ras, &ev);
#endif

	if (mc_ce_trigger && !strcmp(ev.error_type, "Corrected"))
		run_mc_trigger(&ev, mc_ce_trigger);

	if (mc_ue_trigger && !strcmp(ev.error_type, "Uncorrected"))
		run_mc_trigger(&ev, mc_ue_trigger);

	return 0;

parse_error:
	/* FIXME: add a logic here to also store parse errors to SDBD */

	log(ALL, LOG_ERR, "MC error handler: can't parse field #%d\n",
	    parsed_fields);

	return 0;
}
