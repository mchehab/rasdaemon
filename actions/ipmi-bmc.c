// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (C) 2026 Mauro Carvalho Chehab <mchehab+huawei@kernel.org> */

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/wait.h>
#include <unistd.h>

#include "actions/ipmi-bmc.h"
#include "core/modules.h"
#include "core/ras-logger.h"

bool ipmi_bmc_config_enabled(const char *name)
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

static int ipmi_bmc_probe_sel(void)
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

	if (i == nr_devices) {
		log(ALL, LOG_WARNING,
		    "No IPMI device (/dev/ipmi0, /dev/ipmi/0 or /dev/ipmidev/0); BMC SEL reports stay off\n");
		return -ENODEV;
	}

	/* Capture stderr too, avoiding shell diagnostics during the probe. */
	fp = popen("ipmitool sel 2>&1", "r");
	if (!fp) {
		int rc = -errno;

		log(ALL, LOG_WARNING,
		    "Can't run ipmitool: %s; BMC SEL reports stay off\n",
		    strerror(-rc));
		return rc;
	}

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
	if (!WIFEXITED(status) || WEXITSTATUS(status) || !found_version) {
		log(ALL, LOG_WARNING,
		    "ipmitool sel failed or reported no SEL version; BMC SEL reports stay off\n");
		return -ENODEV;
	}

	return 0;
}

int ipmi_bmc_add_sel_entry(const uint8_t *record, size_t size)
{
	char command[256] = "ipmitool raw 0x0a 0x44";
	size_t offset = sizeof("ipmitool raw 0x0a 0x44") - 1;
	int written;

	if (!record || size != IPMI_BMC_SEL_RECORD_SIZE)
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

/*
 * Probe the BMC only when a consumer that needs it is enabled, and only
 * once while the ipmi_bmc module is enabled: probing at module init ran
 * ipmitool on every host with an IPMI device and logged an init failure
 * on every host without one, even with all BMC consumers switched off.
 * Consumers call this after checking that ipmi_bmc is enabled, so its
 * cleanup always resets the result.
 */
static int ipmi_bmc_state; /* 0: not probed, 1: available, -1: not */

bool ipmi_bmc_available(void)
{
	if (!ipmi_bmc_state)
		ipmi_bmc_state = ipmi_bmc_probe_sel() ? -1 : 1;

	return ipmi_bmc_state > 0;
}

static void ipmi_bmc_cleanup(struct ras_module_ctx *ctx)
{
	ipmi_bmc_state = 0;
}

static const struct ras_module_entry ipmi_bmc_module = {
	.name = "ipmi_bmc",
	.level = ACTIONS_MODULE,
	.cleanup = ipmi_bmc_cleanup,
};

REGISTER_RAS_MODULE(ipmi_bmc_module);
