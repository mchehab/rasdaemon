/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (C) 2026 Mauro Carvalho Chehab <mchehab+huawei@kernel.org> */

#ifndef RAS_IPMI_BMC_H
#define RAS_IPMI_BMC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define BMC_GENERIC_ENABLE_ENV "BMC_GENERIC_ENABLE"
#define OPENBMC_UNIFIED_SEL_ENABLE_ENV "OPENBMC_UNIFIED_SEL_ENABLE"
#define AMPERE_OEM_SEL_ENABLE_ENV "AMPERE_OEM_SEL_ENABLE"
#define IPMI_BMC_SEL_RECORD_SIZE 16

bool ipmi_bmc_config_enabled(const char *name);
int ipmi_bmc_add_sel_entry(const uint8_t *record, size_t size);

#endif
