// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * Copyright (c) 2020, Ampere Computing LLC.
 *
 * Code moved from non-standard-ampere.c
 */

#include <stdio.h>
#include <stdlib.h>

#include "actions/ipmi-bmc.h"
#include "core/modules.h"
#include "core/ras-events.h"
#include "core/ras-logger.h"
#include "events/ras-aer-handler.h"

/*
 * This is Ampere's OEM C0 SEL record. It is separate from the OpenBMC
 * unified SEL's 0xfb record and must be enabled only on BMCs that support
 * Ampere's OEM payload.
 */
static int ampere_oem_sel_consume(struct ras_events *ras, int event, void *data)
{
	struct ras_aer_event *aer = data;
	uint8_t record[IPMI_BMC_SEL_RECORD_SIZE] = {
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
		rc = ipmi_bmc_add_sel_entry(record, sizeof(record));
	}
	if (rc)
		log(SYSLOG, LOG_WARNING, "Failed to add Ampere OEM SEL entry\n");

	return 0;
}

static const struct ras_event_consumer ampere_oem_sel_consumer = {
	.name = "ampere-oem-aer",
	.priority = PRI_PLATFORM_ACTION,
	.events = BIT_ULL(AER_EVENT),
	.consume = ampere_oem_sel_consume,
};

static int ampere_oem_sel_init(struct ras_module_ctx *ctx)
{
	int rc;

	if (!ipmi_bmc_config_enabled(AMPERE_OEM_SEL_ENABLE_ENV))
		return 0;

	if (!module_is_enabled("ipmi_bmc"))
		return 0;

	rc = ras_event_consumer_register(&ampere_oem_sel_consumer);
	if (!rc)
		ctx->priv = (void *)&ampere_oem_sel_consumer;
	return rc;
}

static void ampere_oem_sel_cleanup(struct ras_module_ctx *ctx)
{
	if (ctx->priv)
		ras_event_consumer_unregister(&ampere_oem_sel_consumer);
}

static const struct ras_module_entry ampere_oem_sel_module = {
	.name = "ampere-oem-sel",
	.level = ACTIONS_SUB_MODULE,
	.init = ampere_oem_sel_init,
	.cleanup = ampere_oem_sel_cleanup,
};

REGISTER_RAS_MODULE(ampere_oem_sel_module);
