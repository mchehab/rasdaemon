/* SPDX-License-Identifier: GPL-2.0 */

/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2021. All rights reserved.
 */

#ifndef __RAS_CPU_ISOLATION_H
#define __RAS_CPU_ISOLATION_H

#include "queue.h"

#define MAX_BUF_LEN 1024

struct param {
	char *name;
	unsigned long value;
};

struct isolation_param {
	char *name;
	const struct param *units;
	unsigned long value;
	unsigned long limit;
};

enum cpu_state {
	CPU_OFFLINE,
	CPU_ONLINE,
	CPU_OFFLINE_FAILED,
	CPU_UNKNOWN,
};

enum error_handle_result {
	HANDLE_FAILED = -1,
	HANDLE_SUCCEED,
	HANDLE_NOTHING,
};

enum error_type {
	CE = 1,
	UCE
};

struct cpu_info {
	unsigned long uce_nums;
	unsigned long ce_nums;
	struct link_queue *ce_queue;
	enum cpu_state state;
};

struct error_info {
	unsigned long nums;
	time_t time;
	enum error_type err_type;
};

void ras_cpu_isolation_init(unsigned int cpus);
void ras_record_cpu_error(struct error_info *err_info, int cpu);
void cpu_infos_free(void);

#endif
