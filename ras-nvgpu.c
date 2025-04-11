// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * Copyright (C) 2025 Alibaba Inc
 */

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "ras-events.h"
#include "ras-logger.h"
#include "ras-nvgpu.h"
void *ras_nvgpu_handle(void *arg)
{
	(void)arg;
	sigset_t set;
	struct stat st;
	int retry = 3;

	if (stat("/dev/nvidia0", &st) == -1) {
		log(ALL, LOG_WARNING, "NVIDIA device not found: %s\n", strerror(errno));
		return NULL;
	}
	if (!S_ISCHR(st.st_mode)) {
		log(ALL, LOG_WARNING, "NVIDIA device is not a character device\n");
		return NULL;
	}

	sigemptyset(&set);
	sigaddset(&set, SIGINT);
	sigaddset(&set, SIGTERM);
	sigaddset(&set, SIGHUP);
	sigaddset(&set, SIGQUIT);
	if (pthread_sigmask(SIG_BLOCK, &set, NULL) != 0) {
		log(ALL, LOG_ERR, "Failed to set thread signal mask\n");
		return NULL;
	}

	while (retry--) {
		if (ras_nvgpu_nvml_handle()) {
			log(ALL, LOG_ERR, "NVGPU handle retry %d\n", retry);
			sleep(10);
		}
	}

	log(ALL, LOG_ERR, "NVGPU handle fail, exit from nvgpu thread\n");

	return NULL;
}
