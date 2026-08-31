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

static int ipmi_bmc_init(struct ras_module_ctx *ctx)
{
	return ipmi_bmc_probe_sel();
}

static const struct ras_module_entry ipmi_bmc_module = {
	.name = "ipmi_bmc",
	.level = ACTIONS_MODULE,
	.init = ipmi_bmc_init,
};

REGISTER_RAS_MODULE(ipmi_bmc_module);
