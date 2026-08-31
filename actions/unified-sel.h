/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (C) 2026 Mauro Carvalho Chehab <mchehab+huawei@kernel.org> */

#ifndef RAS_IPMITOOL_H
#define RAS_IPMITOOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define IPMITOOL_ENABLE_ENV "IPMITOOL_ENABLE"
#define AMPERE_OEM_SEL_ENABLE_ENV "AMPERE_OEM_SEL_ENABLE"
#define IPMI_SEL_RECORD_SIZE 16

bool ipmitool_config_enabled(const char *name);
int ipmitool_probe_sel(void);
int ipmitool_add_sel_entry(const uint8_t *record, size_t size);

#endif
