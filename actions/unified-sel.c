// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2023, Meta Platforms Inc.
 */

#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/wait.h>
#include <unistd.h>

#include "actions/unified-sel.h"
#include "core/modules.h"
#include "core/ras-events.h"
#include "core/ras-logger.h"
#include "events/ras-aer-handler.h"

/* CPU Root Port Error ID corresponding to each status bit set */
static const char *cor_error_ids[32] = {
	/* Correctable errors */
	[0]  = "0x00", /* Receiver Error */
	[6]  = "0x01", /* Bad TLP */
	[7]  = "0x02", /* Bad DLLP */
	[8]  = "0x04", /* RELAY_NUM Rollover */
	[12] = "0x03", /* Replay Timer Timeout */
	[13] = "0x05", /* Advisory Non-Fatal */
	[14] = "0x06", /* Corrected Internal */
	[15] = "0x07", /* Header Log Overflow */
};

static const char *uncor_error_ids[32] = {
	/* Uncorrectable errors */
	[4] = "0x20",  /* Data Link Protocol */
	[5] = "0x21",  /* Surprise Down Error */
	[12] = "0x22", /* Received Poisoned TLP */
	[13] = "0x23", /* Flow Control Protocol */
	[14] = "0x24", /* Completion Timeout */
	[15] = "0x25", /* Completer Abort */
	[16] = "0x26", /* Unexpected Completion */
	[17] = "0x27", /* Receiver Overflow */
	[18] = "0x28", /* Malformed TLP */
	[19] = "0x29", /* ECRC */
	[20] = "0x2A", /* Unsupported Request */
	[21] = "0x2B", /* ACS Violation */
	[22] = "0x2C", /* Uncorrected Internal */
	[23] = "0x2D", /* MC Blocked TLP */
	[24] = "0x2E", /* AtomicOp Egress Blocked */
	[25] = "0x2F", /* TLP Prefix Blocked */
};

bool ipmitool_config_enabled(const char *name)
{
	const char *value = getenv(name);

	if (!value || !*value || !strcasecmp(value, "no") ||
	    !strcasecmp(value, "false") || !strcasecmp(value, "off") ||
	    !strcmp(value, "0"))
		return false;

	if (!strcasecmp(value, "yes") || !strcasecmp(value, "true") ||
	    !strcasecmp(value, "on") || !strcmp(value, "1"))
		return true;

	log(ALL, LOG_WARNING,
	    "Ignoring invalid %s=%s; use yes or no\n", name, value);
	return false;
}

int ipmitool_probe_sel(void)
{
	static const char * const ipmi_devices[] = {
		"/dev/ipmi0",
		"/dev/ipmi/0",
		"/dev/ipmidev/0",
	};
	char output[256];
	bool found_version = false;
	FILE *fp;
	int status;
	size_t i;
	size_t nr_devices = sizeof(ipmi_devices) / sizeof(*ipmi_devices);

	for (i = 0; i < nr_devices; i++)
		if (!access(ipmi_devices[i], F_OK))
			break;

	if (i == nr_devices)
		return -ENODEV;

	/* Capture stderr too, avoiding shell diagnostics during the probe. */
	fp = popen("ipmitool sel 2>&1", "r");
	if (!fp)
		return -errno;

	while (fgets(output, sizeof(output), fp)) {
		char *p = output;

		while (isspace((unsigned char)*p))
			p++;
		if (!strncmp(p, "Version", sizeof("Version") - 1))
			found_version = true;
	}

	status = pclose(fp);
	if (status < 0)
		return -errno;
	if (!WIFEXITED(status) || WEXITSTATUS(status) || !found_version)
		return -ENODEV;

	return 0;
}

int ipmitool_add_sel_entry(const uint8_t *record, size_t size)
{
	char command[256] = "ipmitool raw 0x0a 0x44";
	size_t offset = sizeof("ipmitool raw 0x0a 0x44") - 1;
	int written;

	if (!record || size != IPMI_SEL_RECORD_SIZE)
		return -EINVAL;

	for (size_t i = 0; i < size; i++) {
		written = snprintf(command + offset, sizeof(command) - offset,
				   " 0x%02x", record[i]);
		if (written < 0 || (size_t)written >= sizeof(command) - offset)
			return -EOVERFLOW;
		offset += written;
	}

	return system(command) ? -EIO : 0;
}

static int verify_id_log_sel(uint64_t status,
			     const char **idarray,
			     unsigned int bus,
			     unsigned int dev_fn)
{
	uint8_t record[IPMI_SEL_RECORD_SIZE] = {
		[2] = 0xfb,
		[3] = 0x20,
		[8] = 0x01,
		[12] = 0x01,
		[14] = 0xff,
	};
	int i;

	/*
	 * Get PCIe AER error source bus/dev/fn and save it to the BMC SEL
	 * as a OpenBMC unified SEL record type.
	 * The IPMI command and record fields are defined in IPMI Specification v2.0 (IPMI Spec)
	 * ipmitool raw 0x0a 0x44 is "Add SEL Entry Command" defined in IPMI spec chapter 31.6
	 * The 16 byte that follow form the SEL Record
	 * defined in IPMI spec chapter 32.1 "SEL Event Records"
	 * Byte 1~2 are Record ID = 0x00 0x00, unused
	 * Byte 3 is Record Type = 0xFB, OEM non-timestamped record type for OpenBMC unified SEL
	 * Byte 4~16 are OEM defined
	 * Byte 11:
	 * Byte11[7:3] Device#
	 * Byte11[2:0] Function#
	 * Byte 12: Bus number
	 * Byte 13-15: Reserved
	 * Byte 16: ID of the error detected on the PCle device that triggered this SEL record
	 */

	/*
	 * Potentially all error status bits could be set for a given PCIe
	 * device. Therefore, iterate over all 32 bits each of cor and uncor
	 * errors.
	 */
	for (i = 0; i < 32; i++) {
		if ((status & (1ULL << i)) && idarray[i]) {
			record[10] = dev_fn;
			record[11] = bus;
			record[15] = strtoul(idarray[i], NULL, 0);
			if (ipmitool_add_sel_entry(record, sizeof(record)))
				return -1;
		}
	}
	return 0;
}

static int openbmc_unified_sel_log(uint64_t severity, const char *dev_name,
				   uint64_t status)
{
	int bus, dev, dev_fn, fn;

	if (sscanf(dev_name, "%*x:%x:%x.%x", &bus, &dev, &fn) != 3)
		return -1;

	dev_fn = (((dev & 0x1f) << 3) | (fn & 0x7));

	/*
	 * Use the appropriate correctable error status ID
	 * for a given severity level
	 */
	if (severity == HW_EVENT_AER_CORRECTED) {
		if (verify_id_log_sel(status, cor_error_ids, bus, dev_fn) < 0)
			return -1;
	} else {
		if (verify_id_log_sel(status, uncor_error_ids, bus, dev_fn) < 0)
			return -1;
	}
	return 0;
}

static int openbmc_unified_sel_consume(struct ras_events *ras, int event,
				       void *data)
{
	struct ras_aer_event *aer = data;

	return openbmc_unified_sel_log(aer->severity, aer->dev_name,
				       aer->status);
}

static const struct ras_event_consumer openbmc_unified_sel_consumer = {
	.name = "openbmc-unified-sel",
	.priority = PRI_PLATFORM_ACTION,
	.events = BIT_ULL(AER_EVENT),
	.consume = openbmc_unified_sel_consume,
};

static int openbmc_unified_sel_init(struct ras_module_ctx *ctx)
{
	int rc;

	if (!ipmitool_config_enabled(IPMITOOL_ENABLE_ENV))
		return 0;

	rc = ipmitool_probe_sel();
	if (rc) {
		log(ALL, LOG_WARNING,
		    "IPMI SEL reporting is disabled: no local IPMI device, or ipmitool sel did not return a Version\n");
		return 0;
	}

	rc = ras_event_consumer_register(&openbmc_unified_sel_consumer);
	if (!rc)
		ctx->priv = (void *)&openbmc_unified_sel_consumer;
	return rc;
}

static void openbmc_unified_sel_cleanup(struct ras_module_ctx *ctx)
{
	if (ctx->priv)
		ras_event_consumer_unregister(&openbmc_unified_sel_consumer);
}

static const struct ras_module_entry openbmc_unified_sel_module = {
	.name = "openbmc-unified-sel",
	.level = ACTIONS_MODULE,
	.init = openbmc_unified_sel_init,
	.cleanup = openbmc_unified_sel_cleanup,
};

REGISTER_RAS_MODULE(openbmc_unified_sel_module);
