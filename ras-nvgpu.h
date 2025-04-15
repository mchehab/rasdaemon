/* SPDX-License-Identifier: GPL-2.0-or-later */

/*
 * Copyright (C) 2025 Alibaba Inc
 */

#ifndef __RAS_NVGPU_H
#define __RAS_NVGPU_H

#define NVGPU_EVENT_NAME "nvgpu"

void *ras_nvgpu_handle(void *arg);
int ras_nvgpu_nvml_handle(void);
#endif
