// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * Copyright (c) 2020, Ampere Computing LLC.
 *
 * Code moved from non-standard-ampere.c
 */

#include <stdio.h>
#include <stdlib.h>

#include "actions/unified-sel.h"
#include "core/modules.h"
#include "core/ras-events.h"
#include "core/ras-logger.h"
#include "events/ras-aer-handler.h"

static int amp_oem_aer_consume(struct ras_events *ras, int event, void *data)
{
	struct ras_aer_event *aer = data;
	uint8_t record[IPMI_SEL_RECORD_SIZE] = {
		[2] = 0xc0,
		[7] = 0x3a,
		[8] = 0xcd,
		[10] = 0xc0,
	};
	uint8_t sensor;
	int seg, bus, dev, fn, rc;

	if (aer->severity == HW_EVENT_AER_UNCORRECTED_NON_FATAL ||
	    aer->severity == HW_EVENT_AER_UNCORRECTED_FATAL)
		sensor = 0xca;
	else
		sensor = 0xbf;

	rc = sscanf(aer->dev_name, "%x:%x:%x.%x", &seg, &bus, &dev, &fn);
	if (rc == 4) {
		record[11] = sensor;
		record[12] = seg & 0xff;
		record[13] = (seg & 0xff00) >> 8;
		record[14] = bus;
		record[15] = ((dev & 0x1f) << 3) | (fn & 0x7);
		rc = ipmitool_add_sel_entry(record, sizeof(record));
	}
	if (rc)
		log(SYSLOG, LOG_WARNING, "Failed to execute ipmitool\n");

	return 0;
}

static const struct ras_event_consumer amp_oem_aer_consumer = {
	.name = "ampere-oem-aer",
	.priority = PRI_PLATFORM_ACTION,
	.events = BIT_ULL(AER_EVENT),
	.consume = amp_oem_aer_consume,
};

static int amp_oem_action_init(struct ras_module_ctx *ctx)
{
	int rc;

	if (!ipmitool_config_enabled(AMPERE_OEM_SEL_ENABLE_ENV))
		return 0;

	rc = ipmitool_probe_sel();
	if (rc) {
		log(ALL, LOG_WARNING,
		    "Ampere OEM SEL reporting is disabled: no local IPMI device, or ipmitool sel did not return a Version\n");
		return 0;
	}

	rc = ras_event_consumer_register(&amp_oem_aer_consumer);
	if (!rc)
		ctx->priv = (void *)&amp_oem_aer_consumer;
	return rc;
}

static void amp_oem_action_cleanup(struct ras_module_ctx *ctx)
{
	if (ctx->priv)
		ras_event_consumer_unregister(&amp_oem_aer_consumer);
}

static const struct ras_module_entry amp_oem_action_module = {
	.name = "ampere-oem-action",
	.level = ACTIONS_SUB_MODULE,
	.init = amp_oem_action_init,
	.cleanup = amp_oem_action_cleanup,
};

REGISTER_RAS_MODULE(amp_oem_action_module);
