// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * Copyright (C) 2013 Mauro Carvalho Chehab <mchehab+huawei@kernel.org>
 */

#include <pci/pci.h>
#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <traceevent/kbuffer.h>
#include <unistd.h>

#include "core/bitfield.h"
#include "core/modules.h"
#include "core/ras-logger.h"
#include "core/trigger.h"
#include "core/types.h"
#include "db/ras-db.h"
#include "events/ras-aer-handler.h"

int ras_aer_event_handler(struct trace_seq *s, struct tep_record *record,
			  struct tep_event *event, void *context);
int db_aer_event(struct ras_events *ras, void *priv);

#ifdef HAVE_UNITTEST
int test_aer(void) __attribute__((weak));
#endif

static const struct ras_event_entry ras_aer_event = {
	.group = "ras", .event = "aer_event",
	.handler = ras_aer_event_handler, .id = AER_EVENT,
	.trigger_setup = aer_event_trigger_setup,
#ifdef HAVE_UNITTEST
	.test_group = TEST_GROUP_EVENTS, .test = test_aer,
#endif
	.record = db_aer_event,
};
REGISTER_RAS_EVENT(ras_aer_event);

/* bit field meaning for correctable error */
static const char *aer_cor_errors[32] = {
	/* Correctable errors */
	[0]  = "Receiver Error",
	[6]  = "Bad TLP",
	[7]  = "Bad DLLP",
	[8]  = "RELAY_NUM Rollover",
	[12] = "Replay Timer Timeout",
	[13] = "Advisory Non-Fatal",
	[14] = "Corrected Internal Error",
	[15] = "Header Log Overflow",
};

/* bit field meaning for uncorrectable error */
static const char *aer_uncor_errors[32] = {
	/* Uncorrectable errors */
	[4]  = "Data Link Protocol",
	[5]  = "Surprise Link Down",
	[12] = "Poisoned TLP",
	[13] = "Flow Control Protocol",
	[14] = "Completion Timeout",
	[15] = "Completer Abort",
	[16] = "Unexpected Completion",
	[17] = "Receiver Overflow",
	[18] = "Malformed TLP",
	[19] = "ECRC",
	[20] = "Unsupported Request",
	[21] = "ACS Violation",
	[22] = "Uncorrected Internal",
	[23] = "MC Blocked TLP",
	[24] = "AtomicOp Egress Blocked",
	[25] = "TLP Prefix Blocked",
	[26] = "Poisoned TLP Egrees Blocked",
};

#define MAX_ENV 30
static const char *aer_ce_trigger = NULL;
static const char *aer_ue_trigger = NULL;

void aer_event_trigger_setup(void)
{
	const char *trigger;

	trigger = getenv("AER_CE_TRIGGER");
	if (trigger && strcmp(trigger, "")) {
		aer_ce_trigger = trigger_check(trigger);

		if (!aer_ce_trigger) {
			log(ALL, LOG_ERR,
			    "Cannot access aer_event ce trigger `%s`\n",
			    trigger);
		} else {
			log(ALL, LOG_INFO,
			    "Setup aer_event ce trigger `%s`\n",
			    trigger);
		}
	} else {
		log(TERM, LOG_ERR, "\t no AER_CE_TRIGGER (%p)\n", trigger);
	}

	trigger = getenv("AER_UE_TRIGGER");
	if (trigger && strcmp(trigger, "")) {
		aer_ue_trigger = trigger_check(trigger);

		if (!aer_ue_trigger) {
			log(ALL, LOG_ERR,
			    "Cannot access aer_event ue trigger `%s`\n",
			    trigger);
		} else {
			log(ALL, LOG_INFO,
			    "Setup aer_event ue trigger `%s`\n",
			    trigger);
		}
	}
}

#define BUF_LEN	1024

static void get_pci_dev_name(char *bdf, char *pci_name, ssize_t len, u16 *vendor_id, u16 *device_id)
{
	struct pci_access *pacc;
	struct pci_dev *dev;
	struct pci_filter filter = {0};
	char *err;

	if (!pci_name)
		return;

	pacc = pci_alloc();
	if (!pacc)
		return;

	pci_init(pacc);
	pci_scan_bus(pacc);
	pci_filter_init(pacc, &filter);
	err = pci_filter_parse_slot(&filter, bdf);
	if (err) {
		log(TERM, LOG_ERR, "Invalid PCI device name %s\n", bdf);
		goto free;
	}

	for (dev = pacc->devices; dev; dev = dev->next) {
		if (pci_filter_match(&filter, dev)) {
			pci_fill_info(dev, PCI_FILL_IDENT);
			*vendor_id = dev->vendor_id;
			*device_id = dev->device_id;
			pci_lookup_name(pacc, pci_name, len,
					PCI_LOOKUP_VENDOR | PCI_LOOKUP_DEVICE,
					dev->vendor_id, dev->device_id);
			break;
		}
	}

free:
	pci_cleanup(pacc);
}

static void run_aer_trigger(struct ras_aer_event *ev, const char *aer_trigger)
{
	char *env[MAX_ENV];
	int ei = 0;
	int i;

	if (asprintf(&env[ei++], "PATH=%s", getenv("PATH") ?: "/sbin:/usr/sbin:/bin:/usr/bin") < 0)
		goto free;
	if (asprintf(&env[ei++], "TIMESTAMP=%s", ev->timestamp) < 0)
		goto free;
	if (asprintf(&env[ei++], "TYPE=%s", ev->error_type) < 0)
		goto free;
	if (asprintf(&env[ei++], "MESSAGE=%s", ev->msg) < 0)
		goto free;
	if (asprintf(&env[ei++], "NAME=%s", ev->dev_name) < 0)
		goto free;
	env[ei] = NULL;
	assert(ei < MAX_ENV);

	run_trigger(aer_trigger, NULL, env, "aer_event");

free:
	for (i = 0; i < ei; i++)
		free(env[i]);
}

int ras_aer_event_handler(struct trace_seq *s,
			  struct tep_record *record,
			  struct tep_event *event, void *context)
{
	int len;
	unsigned long long severity_val;
	unsigned long long status_val;
	unsigned long long val;
	struct ras_events *ras = context;
	time_t now;
	struct tm *tm;
	struct ras_aer_event ev;
	char buf[BUF_LEN] = { 0 };
	uint16_t vendor_id = 0, device_id = 0;
	const char *level;

	if (tep_get_field_val(s, event, "severity", record, &severity_val, 1) < 0)
		return -1;
	ev.severity = severity_val;
	switch (severity_val) {
	case HW_EVENT_AER_UNCORRECTED_NON_FATAL:
		level = loglevel_str[LOGLEVEL_CRIT];
		break;
	case HW_EVENT_AER_UNCORRECTED_FATAL:
		level = loglevel_str[LOGLEVEL_EMERG];
		break;
	case HW_EVENT_AER_CORRECTED:
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

	ev.dev_name = tep_get_field_raw(s, event, "dev_name",
					record, &len, 1);
	if (!ev.dev_name)
		return -1;
	trace_seq_printf(s, "%s ", ev.dev_name);

	get_pci_dev_name(ev.dev_name, buf, sizeof(buf), &vendor_id, &device_id);
	trace_seq_printf(s, "(%s - vendor_id: %#x device_id: %#x) ", buf, vendor_id, device_id);

	if (tep_get_field_val(s,  event, "status", record, &status_val, 1) < 0)
		return -1;
	ev.status = status_val;

	/* Fills the error buffer. If it is a correctable error then use the
	 * aer_cor_errors bit field. Otherwise use aer_uncor_errors.
	 */
	if (severity_val == HW_EVENT_AER_CORRECTED)
		bitfield_msg(buf, sizeof(buf), aer_cor_errors, 32, 0, 0, status_val);
	else
		bitfield_msg(buf, sizeof(buf), aer_uncor_errors, 32, 0, 0, status_val);
	ev.msg = buf;

	if (tep_get_field_val(s, event, "tlp_header_valid",
			      record, &val, 1) < 0)
		return -1;

	ev.tlp_header_valid = val;
	if (ev.tlp_header_valid) {
		ev.tlp_header = tep_get_field_raw(s, event, "tlp_header",
						  record, &len, 1);
		snprintf((buf + strlen(ev.msg)), BUF_LEN - strlen(ev.msg),
			 " TLP Header: %08x %08x %08x %08x",
			 ev.tlp_header[0], ev.tlp_header[1],
			 ev.tlp_header[2], ev.tlp_header[3]);
	}

	trace_seq_printf(s, "%s ", ev.msg);

	/* Use hw_event_aer_err_type switch between different severity_val */
	switch (severity_val) {
	case HW_EVENT_AER_UNCORRECTED_NON_FATAL:
		ev.error_type = "Uncorrected (Non-Fatal)";
		break;
	case HW_EVENT_AER_UNCORRECTED_FATAL:
		ev.error_type = "Uncorrected (Fatal)";
		break;
	case HW_EVENT_AER_CORRECTED:
		ev.error_type = "Corrected";
		break;
	default:
		ev.error_type = "Unknown severity";
	}
	trace_seq_puts(s, ev.error_type);

	ras_event_publish(ras, AER_EVENT, &ev);

	if (aer_ce_trigger && !strcmp(ev.error_type, "Corrected"))
		run_aer_trigger(&ev, aer_ce_trigger);

	if (aer_ue_trigger && !strncmp(ev.error_type, "Uncorrected", 11))
		run_aer_trigger(&ev, aer_ue_trigger);

	return 0;
}
static const struct db_fields aer_event_fields[] = {
	{ .name = "id",			.type = DB_TYPE_SERIAL, .is_pk = true },
	{ .name = "timestamp",		.type = DB_TYPE_TIMESTAMP, .create_index = true },
	{ .name = "dev_name",		.type = DB_TYPE_TEXT },
	{ .name = "err_type",		.type = DB_TYPE_TEXT },
	{ .name = "err_msg",		.type = DB_TYPE_TEXT },
};

const struct db_table_descriptor aer_event_tab = {
	.name = "aer_event",
	.fields = aer_event_fields,
	.num_fields = ARRAY_SIZE(aer_event_fields),
};

static struct db_desc_and_stmt aer_event_db = {
	.desc = &aer_event_tab,
};

int db_aer_event(struct ras_events *ras, void *priv)
{
	struct ras_aer_event *ev = priv;
	int rc, pos = 1;

	if (!aer_event_db.stmt)
		return 0;
	log(TERM, LOG_INFO, "aer_event store: %p\n", aer_event_db.stmt);

	db_bind(&aer_event_tab, aer_event_db.stmt, pos++, (uint64_t)ev->timestamp, -1);
	db_bind(&aer_event_tab, aer_event_db.stmt, pos++, (uint64_t)ev->dev_name, -1);
	db_bind(&aer_event_tab, aer_event_db.stmt, pos++, (uint64_t)ev->error_type, -1);
	db_bind(&aer_event_tab, aer_event_db.stmt, pos++, (uint64_t)ev->msg, -1);

	rc = db_eval_stmt(aer_event_db.stmt, "aer_event");
	if (!rc)
		log(TERM, LOG_INFO, "register inserted at db\n");

	return rc;
}

static int ras_aer_db_init(struct ras_module_ctx *ctx)
{
	return ras_db_table_register(ctx, &aer_event_db);
}

static void ras_aer_db_cleanup(struct ras_module_ctx *ctx)
{
	ras_db_table_unregister(ctx);
}

static const struct ras_module_entry ras_aer_module = {
	.name = "aer-event",
	.level = BASE_EVENT_MODULE,
	.init = ras_aer_db_init,
	.cleanup = ras_aer_db_cleanup,
};

static void __attribute__((constructor)) ras_aer_register(void)
{
	int rc = module_register(&ras_aer_module);

	if (rc)
		log(TERM, LOG_ERR, "Failed to register AER module: %d\n", rc);
}
