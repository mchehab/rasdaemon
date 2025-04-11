// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * Copyright (C) 2025 Alibaba Inc
 */

#include <unistd.h>
#include <stdlib.h>

#include "ras-logger.h"
#include "ras-nvgpu-nvml.h"
#include "ras-nvgpu.h"
#include "trace-seq.h"
#include "types.h"

#define XID_EVENT_NAME "xid"

const char *lib_name[] = {
	"/lib64/libnvidia-ml.so",
	"/lib64/libnvidia-ml.so.1",
	"/usr/local/cuda/targets/x86_64-linux/lib/stubs/libnvidia-ml.so",
	"/usr/local/cuda/targets/sbsa-linux/lib/stubs/libnvidia-ml.so"
};

static void *find_lib(void)
{
	void *handle = NULL;

	for (int i = 0; i < ARRAY_SIZE(lib_name); i++) {
		handle = dlopen(lib_name[i], RTLD_LAZY);
		if (handle)
			return handle;
	}

	log(ALL, LOG_ERR, "Failed to load libnvidia-ml\n");
	return NULL;
}

static int report_ras_gpu_nvml(nvmlEventData_t *data, nvmlDevice_t *devices)
{
	struct trace_seq s;
	nvmlPciInfo_t pci;

	my_nvmlDeviceGetPciInfo(data->device, &pci);

	trace_seq_init(&s);
	if (data->eventType == nvmlEventTypeXidCriticalError) {
		trace_seq_printf(&s, "%16s-%-10d [%03d] %s %6.6f %25s: ",
			"<...>", 0, -1, "....", 0.0f, XID_EVENT_NAME);
		trace_seq_printf(&s, "xid: %lld ", data->eventData);
	} else {
		trace_seq_printf(&s, "%16s-%-10d [%03d] %s %6.6f %25s: ",
			"<...>", 0, -1, "....", 0.0f, NVGPU_EVENT_NAME);
		trace_seq_printf(&s, "event_type: %s ", my_nvmlEventTypeString(data->eventType));
		trace_seq_printf(&s, "data: %lld ", data->eventData);
	}

	trace_seq_printf(&s, "pci_port: " NVML_DEVICE_PCI_BUS_ID_FMT " ", NVML_DEVICE_PCI_BUS_ID_FMT_ARGS(&pci));
	trace_seq_printf(&s, "gpu-i: %x ", data->gpuInstanceId);
	trace_seq_printf(&s, "gpu-ci: %x ", data->computeInstanceId);

	trace_seq_terminate(&s);
	trace_seq_do_printf(&s);
	printf("\n");
	fflush(stdout);
	trace_seq_destroy(&s);

	return 0;
}

int ras_nvgpu_nvml_handle(void)
{
	void *nvml_handle;
	nvmlReturn_t ret;
	unsigned int device_count;
	nvmlDevice_t *devices;
	nvmlEventSet_t event_set;
	char *event_types_str = NULL;
	unsigned long long disable = 0, event_types = 0;
	nvmlEventData_t event_data;

	nvml_handle = find_lib();
	if (!nvml_handle) {
		log(ALL, LOG_ERR, "Failed to load libnvidia-ml: %s\n", dlerror());
		return 1;
	}

	if (my_nvml_setup(nvml_handle)) {
		log(ALL, LOG_ERR, "Failed to setup libnvidia-ml\n");
		dlclose(nvml_handle);
		return 1;
	}

	ret = my_nvmlInit();
	if (ret) {
		log(ALL, LOG_ERR, "NVML Init failed: %s\n", my_nvmlErrorString(ret));
		goto free_dl;
	}

	ret = my_nvmlDeviceGetCount(&device_count);
	if (ret) {
		log(ALL, LOG_ERR, "Get device count failed: %s\n", my_nvmlErrorString(ret));
		goto free_nvml;
	}

	devices = malloc(device_count * sizeof(nvmlDevice_t));
	if (!devices) {
		log(ALL, LOG_ERR, "Failed to allocate memory for devices\n");
		goto free_nvml;
	}

	for (unsigned int i = 0; i < device_count; i++) {
		ret = my_nvmlDeviceGetHandleByIndex(i, &devices[i]);
		if (ret) {
			log(ALL, LOG_ERR, "Get device handle failed: %s\n", my_nvmlErrorString(ret));
			goto free_dev;
		}
	}

	ret = my_nvmlEventSetCreate(&event_set);
	if (ret) {
		log(ALL, LOG_ERR, "Create event set failed: %s\n", my_nvmlErrorString(ret));
		goto free_dev;
	}

	event_types_str = getenv("NVGPU_DISABLE_EVENT");
	if (event_types_str) {
		disable = strtoull(event_types_str, NULL, 0);
		log(ALL, LOG_INFO, "Disable NVGPU events %s\n", my_nvmlEventTypeString(disable));
	}

	for (unsigned int i = 0; i < device_count; i++) {
		ret = my_nvmlDeviceGetSupportedEventTypes(devices[i], &event_types);
		if (ret) {
			log(ALL, LOG_ERR, "Get support events failed: %s\n", my_nvmlErrorString(ret));
			goto free_event;
		}

		ret = my_nvmlDeviceRegisterEvents(devices[i], event_types & ~disable, event_set);
		if (ret) {
			log(ALL, LOG_ERR, "Register events failed: %s\n", my_nvmlErrorString(ret));
			goto free_event;
		}
	}

	while (1) {
		ret = my_nvmlEventSetWait(event_set, &event_data, -1);
		if (!ret)
			report_ras_gpu_nvml(&event_data, devices);
		else {
			log(ALL, LOG_ERR, "Wait for event failed: %s\n", my_nvmlErrorString(ret));
			break;
		}
	}

free_event:
	my_nvmlEventSetFree(event_set);
free_dev:
	free(devices);
free_nvml:
	my_nvmlShutdown();
free_dl:
	dlclose(nvml_handle);
	return ret;
}
