// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (C) 2026 Mauro Carvalho Chehab <mchehab+huawei@kernel.org> */

/*
 * Generic PCIe AER SEL reporting uses an IPMI system-event record (type 0x02).
 * See the IPMI v2.0 specification, section 31.6 (Add SEL Entry), section 32.1
 * (SEL Event Records), and the Critical Interrupt sensor event table. Corrected,
 * non-fatal, and fatal PCIe AER events map to the Bus Correctable (0x07), Bus
 * Uncorrectable (0x08), and Bus Fatal (0x0a) offsets, respectively.
 *
 * A standard SEL sensor number normally identifies a BMC SDR entry. There is
 * no portable PCIe AER sensor number, so this consumer uses zero. Before
 * enabling it, check ``ipmitool sdr elist full`` for a Critical Interrupt
 * sensor numbered 00h. Event data 2 and 3 are marked as OEM data and retain
 * the PCI bus and device/function; BMC firmware may store that data without
 * decoding it.
 *
 * Platform firmware can handle AER first and log a corresponding SEL entry
 * itself. Enable this consumer only after checking that it will not duplicate
 * firmware-generated PCIe AER records on the target platform.
 */

#include <errno.h>
#include <stdio.h>

#include "actions/ipmi-bmc.h"
#include "core/modules.h"
#include "core/ras-events.h"
#include "core/ras-logger.h"
#include "events/ras-aer-handler.h"

static int bmc_generic_aer_log(uint64_t severity, const char *dev_name,
			       uint64_t status)
{
	uint8_t record[IPMI_BMC_SEL_RECORD_SIZE] = {
		[2] = 0x02, /* System event record */
		[7] = 0x01, /* Software ID */
		[9] = 0x04, /* IPMI 2.0 event message revision */
		[10] = 0x13, /* Critical Interrupt sensor */
		[12] = 0x6f, /* Sensor-specific discrete event */
	};
	unsigned int domain, bus, device, function;

	if (!status)
		return 0;

	if (sscanf(dev_name, "%x:%x:%x.%x", &domain, &bus, &device,
		   &function) != 4 || device > 0x1f || function > 0x7)
		return -EINVAL;

	switch (severity) {
	case HW_EVENT_AER_CORRECTED:
		record[13] = 0xa7; /* Bus Correctable, OEM data follows */
		break;
	case HW_EVENT_AER_UNCORRECTED_NON_FATAL:
		record[13] = 0xa8; /* Bus Uncorrectable, OEM data follows */
		break;
	case HW_EVENT_AER_UNCORRECTED_FATAL:
		record[13] = 0xaa; /* Bus Fatal, OEM data follows */
		break;
	default:
		return -EINVAL;
	}

	/* Event data 2/3 are OEM data: PCI bus and device/function. */
	record[14] = bus;
	record[15] = (device << 3) | function;

	return ipmi_bmc_add_sel_entry(record, sizeof(record));
}

static int bmc_generic_aer_consume(struct ras_events *ras, int event, void *data)
{
	struct ras_aer_event *aer = data;
	int rc;

	rc = bmc_generic_aer_log(aer->severity, aer->dev_name, aer->status);
	if (rc)
		log(SYSLOG, LOG_WARNING, "Failed to add IPMI BMC SEL entry\n");

	return 0;
}

static const struct ras_event_consumer bmc_generic_aer_consumer = {
	.name = "bmc-generic",
	.priority = PRI_PLATFORM_ACTION,
	.events = BIT_ULL(AER_EVENT),
	.consume = bmc_generic_aer_consume,
};

static int bmc_generic_init(struct ras_module_ctx *ctx)
{
	int rc;

	if (!ipmi_bmc_config_enabled(BMC_GENERIC_ENABLE_ENV) ||
	    !module_is_enabled("ipmi_bmc"))
		return 0;

	rc = ras_event_consumer_register(&bmc_generic_aer_consumer);
	if (!rc)
		ctx->priv = (void *)&bmc_generic_aer_consumer;
	return rc;
}

static void bmc_generic_cleanup(struct ras_module_ctx *ctx)
{
	if (ctx->priv)
		ras_event_consumer_unregister(&bmc_generic_aer_consumer);
}

static const struct ras_module_entry bmc_generic_module = {
	.name = "bmc-generic",
	.level = ACTIONS_SUB_MODULE,
	.init = bmc_generic_init,
	.cleanup = bmc_generic_cleanup,
};

REGISTER_RAS_MODULE(bmc_generic_module);
