// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * Copyright (c) 2020, Ampere Computing LLC.
 *
 * Code moved from non-standard-ampere.c
 */

#include <stdio.h>
#include <stdlib.h>

#include "core/modules.h"
#include "core/ras-events.h"
#include "core/ras-logger.h"
#include "events/ras-aer-handler.h"

static int amp_oem_aer_consume(struct ras_events *ras, int event, void *data)
{
	struct ras_aer_event *aer = data;
	char ipmi_add_sel[105];
	uint8_t sensor;
	int seg, bus, dev, fn, rc;

	if (aer->severity == HW_EVENT_AER_UNCORRECTED_NON_FATAL ||
	    aer->severity == HW_EVENT_AER_UNCORRECTED_FATAL)
		sensor = 0xca;
	else
		sensor = 0xbf;

	rc = sscanf(aer->dev_name, "%x:%x:%x.%x", &seg, &bus, &dev, &fn);
	if (rc == 4) {
		snprintf(ipmi_add_sel, sizeof(ipmi_add_sel),
			 "ipmitool raw 0x0a 0x44 0x00 0x00 0xc0 0x00 0x00 0x00 0x00 0x3a 0xcd 0x00 0xc0 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x",
			 sensor, seg & 0xff, (seg & 0xff00) >> 8, bus,
			 ((dev & 0x1f) << 3) | (fn & 0x7));
		rc = system(ipmi_add_sel);
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
	return ras_event_consumer_register(&amp_oem_aer_consumer);
}

static void amp_oem_action_cleanup(struct ras_module_ctx *ctx)
{
	ras_event_consumer_unregister(&amp_oem_aer_consumer);
}

static const struct ras_module_entry amp_oem_action_module = {
	.name = "ampere-oem-action",
	.level = ACTIONS_MODULE,
	.init = amp_oem_action_init,
	.cleanup = amp_oem_action_cleanup,
};

static void __attribute__((constructor)) amp_oem_action_register(void)
{
	int rc = module_register(&amp_oem_action_module);

	if (rc)
		log(TERM, LOG_ERR,
		    "Failed to register Ampere OEM action module: %d\n", rc);
}
