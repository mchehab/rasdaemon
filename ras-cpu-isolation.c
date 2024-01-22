/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2021. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <limits.h>
#include <ctype.h>
#include "ras-logger.h"
#include "ras-cpu-isolation.h"

#define SECOND_OF_MON (30 * 24 * 60 * 60)
#define SECOND_OF_DAY (24 * 60 * 60)
#define SECOND_OF_HOU (60 * 60)
#define SECOND_OF_MIN (60)

#define LIMIT_OF_CPU_THRESHOLD 10000
#define INIT_OF_CPU_THRESHOLD 18
#define DEC_CHECK 10
#define LAST_BIT_OF_UL 5

static struct cpu_info *cpu_infos;
static unsigned int ncores;
static unsigned int enabled = 1;
static const char *cpu_path_format = "/sys/devices/system/cpu/cpu%d/online";

static const struct param normal_units[] = {
	{"", 1},
	{}
};

static const struct param cycle_units[] = {
	{"d", SECOND_OF_DAY},
	{"h", SECOND_OF_HOU},
	{"m", SECOND_OF_MIN},
	{"s", 1},
	{}
};

static struct isolation_param threshold = {
	.name = "CPU_CE_THRESHOLD",
	.units = normal_units,
	.value = INIT_OF_CPU_THRESHOLD,
	.limit = LIMIT_OF_CPU_THRESHOLD
};

static struct isolation_param cpu_limit = {
	.name = "CPU_ISOLATION_LIMIT",
	.units = normal_units
};

static struct isolation_param cycle = {
	.name = "CPU_ISOLATION_CYCLE",
	.units = cycle_units,
	.value = SECOND_OF_DAY,
	.limit = SECOND_OF_MON
};

static const char * const cpu_state[] = {
	[CPU_OFFLINE] = "offline",
	[CPU_ONLINE] = "online",
	[CPU_OFFLINE_FAILED] = "offline-failed",
	[CPU_UNKNOWN] = "unknown"
};

static int open_sys_file(unsigned int cpu, int __oflag, const char *format)
{
	int fd;
	char path[PATH_MAX] = "";
	char real_path[PATH_MAX] = "";

	snprintf(path, sizeof(path), format, cpu);
	if (strlen(path) > PATH_MAX || realpath(path, real_path) == NULL) {
		log(TERM, LOG_ERR, "[%s]:open file: %s failed\n", __func__, path);
		return -1;
	}
	fd = open(real_path, __oflag);
	if (fd == -1) {
		log(TERM, LOG_ERR, "[%s]:open file: %s failed\n", __func__, real_path);
		return -1;
	}

	return fd;
}

static int get_cpu_status(unsigned int cpu)
{
	int fd, num;
	char buf[2] = "";

	fd = open_sys_file(cpu, O_RDONLY, cpu_path_format);
	if (fd == -1)
		return CPU_UNKNOWN;

	if (read(fd, buf, 1) <= 0 || sscanf(buf, "%d", &num) != 1)
		num = CPU_UNKNOWN;

	close(fd);

	return (num < 0 || num > CPU_UNKNOWN) ? CPU_UNKNOWN : num;
}

static int init_cpu_info(unsigned int cpus)
{
	ncores = cpus;
	cpu_infos = (struct cpu_info *)malloc(sizeof(*cpu_infos) * cpus);
	if (!cpu_infos) {
		log(TERM, LOG_ERR,
		    "Failed to allocate memory for cpu infos in %s.\n", __func__);
		return -1;
	}

	for (unsigned int i = 0; i < cpus; ++i) {
		cpu_infos[i].ce_nums = 0;
		cpu_infos[i].uce_nums = 0;
		cpu_infos[i].state = get_cpu_status(i);
		cpu_infos[i].ce_queue = init_queue();

		if (!cpu_infos[i].ce_queue) {
			log(TERM, LOG_ERR,
			    "Failed to allocate memory for cpu ce queue in %s.\n", __func__);
			return -1;
		}
	}
	/* set limit of offlined cpu limit according to number of cpu */
	cpu_limit.limit = cpus - 1;
	cpu_limit.value = 0;

	return 0;
}

static void check_config(struct isolation_param *config)
{
	if (config->value > config->limit) {
		log(TERM, LOG_WARNING, "Value: %lu exceed limit: %lu, set to limit\n",
		    config->value, config->limit);
		config->value = config->limit;
	}
}

static int parse_ul_config(struct isolation_param *config, char *env, unsigned long *value)
{
	char *unit = NULL;
	int env_size, has_unit = 0;

	if (!env || strlen(env) == 0)
		return -1;

	env_size = strlen(env);
	unit = env + env_size - 1;

	if (isalpha(*unit)) {
		has_unit = 1;
		env_size--;
		if (env_size <= 0)
			return -1;
	}

	for (int i = 0; i < env_size; ++i) {
		if (isdigit(env[i])) {
			if (*value > ULONG_MAX / DEC_CHECK ||
			    (*value == ULONG_MAX / DEC_CHECK && env[i] - '0' > LAST_BIT_OF_UL)) {
				log(TERM, LOG_ERR, "%s is out of range: %lu\n", env, ULONG_MAX);
				return -1;
			}
			*value = DEC_CHECK * (*value) + (env[i] - '0');
		} else
			return -1;
	}

	if (!has_unit)
		return 0;

	for (const struct param *units = config->units; units->name; units++) {
		/* value character and unit character are both valid */
		if (!strcasecmp(unit, units->name)) {
			if (*value > (ULONG_MAX / units->value)) {
				log(TERM, LOG_ERR,
				    "%s is out of range: %lu\n", env, ULONG_MAX);
				return -1;
			}
			*value = (*value) * units->value;
			return 0;
		}
	}
	log(TERM, LOG_ERR, "Invalid unit %s\n", unit);
	return -1;
}

static void init_config(struct isolation_param *config)
{
	char *env = getenv(config->name);
	unsigned long value = 0;

	if (parse_ul_config(config, env, &value) < 0) {
		log(TERM, LOG_ERR, "Invalid %s: %s! Use default value %lu.\n",
		    config->name, env, config->value);
		return;
	}

	config->value = value;
	check_config(config);
}

static int check_config_status(void)
{
	char *env = getenv("CPU_ISOLATION_ENABLE");

	if (!env || strcasecmp(env, "yes"))
		return -1;

	return 0;
}

void ras_cpu_isolation_init(unsigned int cpus)
{
	if (init_cpu_info(cpus) < 0 || check_config_status() < 0) {
		enabled = 0;
		log(TERM, LOG_WARNING, "Cpu fault isolation is disabled\n");
		return;
	}

	log(TERM, LOG_INFO, "Cpu fault isolation is enabled\n");
	init_config(&threshold);
	init_config(&cpu_limit);
	init_config(&cycle);
}

void cpu_infos_free(void)
{
	if (cpu_infos) {
		for (int i = 0; i < ncores; ++i)
			free_queue(cpu_infos[i].ce_queue);

		free(cpu_infos);
	}
}

static int do_cpu_offline(unsigned int cpu)
{
	int fd, rc;
	char buf[2] = "";

	cpu_infos[cpu].state = CPU_OFFLINE_FAILED;
	fd = open_sys_file(cpu, O_RDWR, cpu_path_format);
	if (fd == -1)
		return HANDLE_FAILED;

	strcpy(buf, "0");
	rc = write(fd, buf, strlen(buf));
	if (rc < 0) {
		log(TERM, LOG_ERR, "cpu%u offline failed, errno:%d\n", cpu, errno);
		close(fd);
		return HANDLE_FAILED;
	}

	close(fd);
	/* check wthether the cpu is isolated successfully */
	cpu_infos[cpu].state = get_cpu_status(cpu);

	if (cpu_infos[cpu].state == CPU_OFFLINE)
		return HANDLE_SUCCEED;

	return HANDLE_FAILED;
}

static int do_ce_handler(unsigned int cpu)
{
	struct link_queue *queue = cpu_infos[cpu].ce_queue;
	unsigned int tmp;
	/*
	 * Since we just count all error numbers in setted cycle, we store the time
	 * and error numbers from current event to the queue, then everytime we
	 * calculate the period from beginning time to ending time, if the period
	 * exceeds setted cycle, we pop the beginning time and error until the period
	 * from new beginning time to ending time is less than cycle.
	 */
	while (queue->head && queue->tail && queue->tail->time - queue->head->time > cycle.value) {
		tmp = queue->head->value;
		if (pop(queue) == 0)
			cpu_infos[cpu].ce_nums -= tmp;
	}
	log(TERM, LOG_INFO,
	    "Current number of Corrected Errors in cpu%d in the cycle is %lu\n",
		cpu, cpu_infos[cpu].ce_nums);

	if (cpu_infos[cpu].ce_nums >= threshold.value) {
		log(TERM, LOG_INFO,
		    "Corrected Errors exceeded threshold %lu, try to offline cpu%u\n",
			threshold.value, cpu);
		return do_cpu_offline(cpu);
	}
	return HANDLE_NOTHING;
}

static int do_uce_handler(unsigned int cpu)
{
	if (cpu_infos[cpu].uce_nums > 0) {
		log(TERM, LOG_INFO, "Uncorrected Errors occurred, try to offline cpu%u\n", cpu);
		return do_cpu_offline(cpu);
	}
	return HANDLE_NOTHING;
}

static int error_handler(unsigned int cpu, struct error_info *err_info)
{
	int ret = HANDLE_NOTHING;

	switch (err_info->err_type) {
	case CE:
		ret = do_ce_handler(cpu);
		break;
	case UCE:
		ret = do_uce_handler(cpu);
		break;
	default:
		break;
	}

	return ret;
}

static void record_error_info(unsigned int cpu, struct error_info *err_info)
{
	switch (err_info->err_type) {
	case CE:
	{
		struct queue_node *node = node_create(err_info->time, err_info->nums);

		if (!node) {
			log(TERM, LOG_ERR, "Fail to allocate memory for queue node\n");
			return;
		}
		push(cpu_infos[cpu].ce_queue, node);
		cpu_infos[cpu].ce_nums += err_info->nums;
		break;
	}
	case UCE:
		cpu_infos[cpu].uce_nums++;
		break;
	default:
		break;
	}
}

void ras_record_cpu_error(struct error_info *err_info, int cpu)
{
	int ret;

	if (enabled == 0)
		return;

	if (cpu >= ncores || cpu < 0) {
		log(TERM, LOG_ERR,
		    "The current cpu %d has exceed the total number of cpu:%u\n", cpu, ncores);
		return;
	}

	log(TERM, LOG_INFO, "Handling error on cpu%d\n", cpu);
	cpu_infos[cpu].state = get_cpu_status(cpu);

	if (cpu_infos[cpu].state != CPU_ONLINE) {
		log(TERM, LOG_INFO, "Cpu%d is not online or unknown, ignore\n", cpu);
		return;
	}

	record_error_info(cpu, err_info);
	/*
	 * Since user may change cpu state, we get current offlined
	 * cpu numbers every recording time.
	 */
	if (ncores - sysconf(_SC_NPROCESSORS_ONLN) >= cpu_limit.value) {
		log(TERM, LOG_WARNING,
		    "Offlined cpus have exceeded limit: %lu, choose to do nothing\n",
			cpu_limit.value);
		return;
	}

	ret = error_handler(cpu, err_info);
	if (ret == HANDLE_NOTHING)
		log(TERM, LOG_WARNING, "Doing nothing in the cpu%d\n", cpu);
	else if (ret == HANDLE_SUCCEED) {
		log(TERM, LOG_INFO, "Offline cpu%d succeed, the state is %s\n",
		    cpu, cpu_state[cpu_infos[cpu].state]);
		clear_queue(cpu_infos[cpu].ce_queue);
		cpu_infos[cpu].ce_nums = 0;
		cpu_infos[cpu].uce_nums = 0;
	} else
		log(TERM, LOG_WARNING, "Offline cpu%d fail, the state is %s\n",
		    cpu, cpu_state[cpu_infos[cpu].state]);
}
