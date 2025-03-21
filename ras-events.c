// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * Copyright (C) 2013 Mauro Carvalho Chehab <mchehab+huawei@kernel.org>
 */

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
#include <sys/signalfd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <traceevent/event-parse.h>
#include <traceevent/kbuffer.h>
#include <unistd.h>

#include "ras-aer-handler.h"
#include "ras-arm-handler.h"
#include "ras-cpu-isolation.h"
#include "ras-cxl-handler.h"
#include "ras-devlink-handler.h"
#include "ras-diskerror-handler.h"
#include "ras-events.h"
#include "ras-extlog-handler.h"
#include "ras-logger.h"
#include "ras-mce-handler.h"
#include "ras-mc-handler.h"
#include "ras-memory-failure-handler.h"
#include "ras-non-standard-handler.h"
#include "ras-page-isolation.h"
#include "ras-record.h"
#include "trigger.h"

/*
 * Polling time, if read() doesn't block. Currently, trace_pipe_raw never
 * blocks on read(). So, we need to sleep for a while, to avoid spending
 * too much CPU cycles. A fix for it is expected for 3.10.
 */
#define POLLING_TIME 3

/* Test for a little-endian machine */
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
	#define ENDIAN KBUFFER_ENDIAN_LITTLE
#else
	#define ENDIAN KBUFFER_ENDIAN_BIG
#endif

char *choices_disable;

static const struct event_trigger event_triggers[] = {
	{ "mc_event", &mc_event_trigger_setup },
#ifdef HAVE_MEMORY_FAILURE
	{ "memory_failure_event", &mem_fail_event_trigger_setup },
#endif
};

static int get_debugfs_dir(char *tracing_dir, size_t len)
{
	FILE *fp;
	char line[MAX_PATH + 1 + 256];
	char *p, *type, *dir;

	fp = fopen("/proc/mounts", "r");
	if (!fp) {
		log(ALL, LOG_INFO, "Can't open /proc/mounts");
		return -errno;
	}

	do {
		if (!fgets(line, sizeof(line), fp))
			break;

		p = strtok(line, " \t");
		if (!p)
			break;

		dir = strtok(NULL, " \t");
		if (!dir)
			break;

		type = strtok(NULL, " \t");
		if (!type)
			break;

		if (!strcmp(type, "debugfs")) {
			fclose(fp);
			strscpy(tracing_dir, dir, len - 1);
			return 0;
		}
	} while (1);

	fclose(fp);
	log(ALL, LOG_INFO, "Can't find debugfs\n");
	return -ENOENT;
}

static int wait_access(char *path, int ms)
{
	int i;

	for (i = 0; i < ms; i++) {
		if (access(path, F_OK) == 0)
			return 0;
		usleep(1000);
	}

	log(ALL, LOG_WARNING, "%s failed, %s not created in %d ms\n",
	    __func__, path, ms);
	return -1;
}

static int open_trace(struct ras_events *ras, char *name, int flags)
{
	char fname[MAX_PATH + 1];
	int rc;

	rc = strscpy(fname, ras->tracing, sizeof(fname));
	if (rc < 0)
		return rc;
	rc = strscat(fname, "/", sizeof(fname));
	if (rc < 0)
		return rc;
	rc = strscat(fname, name, sizeof(fname));
	if (rc < 0)
		return rc;

	rc = wait_access(fname, 30000);
	if (rc != 0) {
		/* use -1 to keep same error value with open() */
		return -1;
	}

	rc = open(fname, flags);
	if (rc < 0) {
		rc = -errno;

		return -errno;
	}

	return rc;
}

static int get_tracing_dir(struct ras_events *ras)
{
	char		fname[MAX_PATH + 1];
	int		rc, has_instances = 0;
	DIR		*dir;
	struct dirent	*entry;

	get_debugfs_dir(ras->debugfs, sizeof(ras->debugfs));

	rc = strscpy(fname, ras->debugfs, sizeof(fname));
	if (rc < 0)
		return rc;
	rc = strscat(fname, "/tracing", sizeof(fname));
	if (rc < 0)
		return rc;

	dir = opendir(fname);
	if (!dir)
		return -EINVAL;

	for (entry = readdir(dir); entry; entry = readdir(dir)) {
		if (strstr(entry->d_name, "instances")) {
			has_instances = 1;
			break;
		}
	}
	closedir(dir);

	strscpy(ras->tracing, ras->debugfs, sizeof(ras->tracing));
	strscat(ras->tracing, "/tracing", sizeof(ras->tracing));
	if (has_instances) {
		rc = strscat(ras->tracing, "/instances/" TOOL_NAME,
			     sizeof(ras->tracing));
		if (rc < 0)
			return rc;

		rc = mkdir(ras->tracing, 0700);
		if (rc < 0 && errno != EEXIST) {
			log(ALL, LOG_INFO,
			    "Unable to create " TOOL_NAME " instance at %s\n",
			    ras->tracing);
			return -EINVAL;
		}
	}
	return 0;
}

static bool is_disabled_event(char *group, char *event)
{
	char ras_event_name[MAX_PATH + 1];

	snprintf(ras_event_name, sizeof(ras_event_name), "%s:%s",
		 group, event);

	if (choices_disable && strlen(choices_disable) != 0 &&
	    strstr(choices_disable, ras_event_name)) {
		return true;
	}
	return false;
}

/*
 * Tracing enable/disable code
 */
static int __toggle_ras_mc_event(struct ras_events *ras,
				 char *group, char *event, int enable)
{
	int fd, rc;
	char fname[MAX_PATH + 1];

	if (enable)
		enable = is_disabled_event(group, event) ? 0 : 1;

	snprintf(fname, sizeof(fname), "%s%s:%s\n",
		 enable ? "" : "!",
		 group, event);

	/* Enable RAS events */
	fd = open_trace(ras, "set_event", O_RDWR | O_APPEND);
	if (fd < 0) {
		log(ALL, LOG_WARNING, "Can't open set_event\n");
		return -errno;
	}

	rc = write(fd, fname, strlen(fname));
	if (rc < 0) {
		log(ALL, LOG_WARNING, "Can't write to set_event\n");
		close(fd);
		return rc;
	}
	close(fd);
	if (!rc) {
		log(ALL, LOG_WARNING, "Nothing was written on set_event\n");
		return -EIO;
	}

	log(TERM, LOG_DEBUG, "%s:%s event %s\n",
	    group, event,
	    enable ? "enabled" : "disabled");

	return 0;
}

int toggle_ras_mc_event(int enable)
{
	struct ras_events *ras;
	int rc = 0;

	ras = calloc(1, sizeof(*ras));
	if (!ras) {
		log(TERM, LOG_ERR, "Can't allocate memory for ras struct\n");
		return -errno;
	}

	rc = get_tracing_dir(ras);
	if (rc < 0) {
		log(TERM, LOG_ERR, "Can't locate a mounted debugfs\n");
		goto free_ras;
	}

	rc = __toggle_ras_mc_event(ras, "ras", "mc_event", enable);

#ifdef HAVE_AER
	rc |= __toggle_ras_mc_event(ras, "ras", "aer_event", enable);
#endif

#ifdef HAVE_MCE
	rc |= __toggle_ras_mc_event(ras, "mce", "mce_record", enable);
#endif

#ifdef HAVE_EXTLOG
	rc |= __toggle_ras_mc_event(ras, "ras", "extlog_mem_event", enable);
#endif

#ifdef HAVE_NON_STANDARD
	rc |= __toggle_ras_mc_event(ras, "ras", "non_standard_event", enable);
#endif

#ifdef HAVE_ARM
	rc |= __toggle_ras_mc_event(ras, "ras", "arm_event", enable);
#endif

#ifdef HAVE_DEVLINK
	rc |= __toggle_ras_mc_event(ras, "devlink", "devlink_health_report", enable);
#endif

#ifdef HAVE_DISKERROR
#ifdef HAVE_BLK_RQ_ERROR
	rc |= __toggle_ras_mc_event(ras, "block", "block_rq_error", enable);
#else
	rc |= __toggle_ras_mc_event(ras, "block", "block_rq_complete", enable);
#endif
#endif

#ifdef HAVE_MEMORY_FAILURE
	rc |= __toggle_ras_mc_event(ras, "ras", "memory_failure_event", enable);
#endif

#ifdef HAVE_CXL
	rc |= __toggle_ras_mc_event(ras, "cxl", "cxl_poison", enable);
	rc |= __toggle_ras_mc_event(ras, "cxl", "cxl_aer_uncorrectable_error", enable);
	rc |= __toggle_ras_mc_event(ras, "cxl", "cxl_aer_correctable_error", enable);
	rc |= __toggle_ras_mc_event(ras, "cxl", "cxl_overflow", enable);
	rc |= __toggle_ras_mc_event(ras, "cxl", "cxl_generic_event", enable);
	rc |= __toggle_ras_mc_event(ras, "cxl", "cxl_general_media", enable);
	rc |= __toggle_ras_mc_event(ras, "cxl", "cxl_dram", enable);
	rc |= __toggle_ras_mc_event(ras, "cxl", "cxl_memory_module", enable);
#endif

free_ras:
	free(ras);
	if (rc)
		return -EINVAL;

	return 0;
}

static void setup_event_trigger(char *event)
{
	struct event_trigger trigger;

	for (int i = 0; i < ARRAY_SIZE(event_triggers); i++) {
		trigger = event_triggers[i];
		if (!strcmp(event, trigger.name))
			trigger.setup();
	}
}

#ifdef HAVE_DISKERROR
#ifndef HAVE_BLK_RQ_ERROR
/*
 * Set kernel filter. libtrace doesn't provide an API for setting filters
 * in kernel, we have to implement it here.
 */
static int filter_ras_mc_event(struct ras_events *ras, char *group, char *event,
			       const char *filter_str)
{
	int fd, rc;
	char fname[MAX_PATH + 1];

	snprintf(fname, sizeof(fname), "events/%s/%s/filter", group, event);
	fd = open_trace(ras, fname, O_RDWR | O_APPEND);
	if (fd < 0) {
		log(ALL, LOG_WARNING, "Can't open filter file\n");
		return -errno;
	}

	rc = write(fd, filter_str, strlen(filter_str));
	if (rc < 0) {
		log(ALL, LOG_WARNING, "Can't write to filter file\n");
		close(fd);
		return rc;
	}
	close(fd);
	if (!rc) {
		log(ALL, LOG_WARNING, "Nothing was written on filter file\n");
		return -EIO;
	}

	return 0;
}
#endif
#endif

/*
 * Tracing read code
 */

static int get_pagesize(struct ras_events *ras, struct tep_handle *pevent)
{
	int fd, len, page_size = 8192;
	char buf[page_size];

	fd = open_trace(ras, "events/header_page", O_RDONLY);
	if (fd < 0)
		return page_size;

	len = read(fd, buf, page_size);
	if (len <= 0)
		goto error;
	if (tep_parse_header_page(pevent, buf, len, sizeof(long)))
		goto error;

error:
	close(fd);
	return page_size;
}

static void parse_ras_data(struct pthread_data *pdata, struct kbuffer *kbuf,
			   void *data, unsigned long long time_stamp)
{
	struct tep_record record;
	struct trace_seq s;

	record.ts = time_stamp;
	record.size = kbuffer_event_size(kbuf);
	record.data = data;
	record.offset = kbuffer_curr_offset(kbuf);
	record.cpu = pdata->cpu;

	/* note offset is just offset in subbuffer */
	record.missed_events = kbuffer_missed_events(kbuf);
	record.record_size = kbuffer_curr_size(kbuf);

	/* TODO - logging */
	trace_seq_init(&s);
	tep_print_event(pdata->ras->pevent, &s, &record,
			"%16s-%-5d [%03d] %s %6.1000d %s %s",
			TEP_PRINT_COMM, TEP_PRINT_PID, TEP_PRINT_CPU,
			TEP_PRINT_LATENCY, TEP_PRINT_TIME, TEP_PRINT_NAME,
			TEP_PRINT_INFO);
	trace_seq_do_printf(&s);
	printf("\n");
	fflush(stdout);
	trace_seq_destroy(&s);
}

static int get_num_cpus(struct ras_events *ras)
{
	int cpus;

	cpus = sysconf(_SC_NPROCESSORS_ONLN);
	assert(cpus > 0);
	return cpus;
}

static int set_buffer_percent(struct ras_events *ras, int percent)
{
	char buf[16];
	ssize_t size;
	int res = 0;
	int fd;

	fd = open_trace(ras, "buffer_percent", O_WRONLY);
	if (fd >= 0) {
		/* For the backward compatibility to the old kernels, do not return
		 * if fail to set the buffer_percent.
		 */
		snprintf(buf, sizeof(buf), "%d", percent);
		size = write(fd, buf, strlen(buf));
		if (size <= 0) {
			log(TERM, LOG_WARNING, "can't write to buffer_percent\n");
			res = -EINVAL;
		}
		close(fd);
	} else {
		log(TERM, LOG_WARNING, "Can't open buffer_percent\n");
		res = -EINVAL;
	}

	return res;
}

/*
 * Kernel tracepoint had an incompatible change in 2019, causing polling
 * tracepoints to fail. Rasdaemon can support both legacy and newer versions,
 * with the help of a backup-compatibility legacy kernel mode.
 *
 * The LEGACY_KERNEL flag indicates the need to enable such code.
 */
#define LEGACY_KERNEL		255

static int read_ras_event_all_cpus(struct pthread_data *pdata,
				   unsigned int n_cpus)
{
	ssize_t size;
	unsigned long long time_stamp;
	void *data;
	int ready, i, count_nready;
	struct kbuffer *kbuf;
	void *page;
	struct pollfd fds[n_cpus + 1];
	struct signalfd_siginfo fdsiginfo;
	sigset_t mask;
	int warnonce[n_cpus];
	char pipe_raw[PATH_MAX];
	int legacy_kernel = 0;

	memset(&warnonce, 0, sizeof(warnonce));

	page = malloc(pdata[0].ras->page_size);
	if (!page) {
		log(TERM, LOG_ERR, "Can't allocate page\n");
		return -ENOMEM;
	}

	kbuf = kbuffer_alloc(KBUFFER_LSIZE_8, ENDIAN);
	if (!kbuf) {
		log(TERM, LOG_ERR, "Can't allocate kbuf\n");
		free(page);
		return -ENOMEM;
	}

	/* Fix for poll() on the per_cpu trace_pipe and trace_pipe_raw blocks
	 * indefinitely with the default buffer_percent in the kernel trace system,
	 * which is introduced by the following change in the kernel.
	 * https://lore.kernel.org/all/20221020231427.41be3f26@gandalf.local.home/T/#u.
	 * Set buffer_percent to 0 so that poll() will return immediately
	 * when the trace data is available in the ras per_cpu trace pipe_raw
	 */
	if (set_buffer_percent(pdata[0].ras, 0))
		log(TERM, LOG_WARNING, "Set buffer_percent failed\n");

	for (i = 0; i < (n_cpus + 1); i++)
		fds[i].fd = -1;

	for (i = 0; i < n_cpus; i++) {
		fds[i].events = POLLIN;

		/* FIXME: use select to open for all CPUs */
		snprintf(pipe_raw, sizeof(pipe_raw),
			 "per_cpu/cpu%d/trace_pipe_raw", i);

		fds[i].fd = open_trace(pdata[0].ras, pipe_raw, O_RDONLY);
		if (fds[i].fd < 0) {
			log(TERM, LOG_ERR, "Can't open trace_pipe_raw\n");
			goto error;
		}
	}

	sigemptyset(&mask);
	sigaddset(&mask, SIGINT);
	sigaddset(&mask, SIGTERM);
	sigaddset(&mask, SIGHUP);
	sigaddset(&mask, SIGQUIT);
	if (sigprocmask(SIG_BLOCK, &mask, NULL) == -1)
		log(TERM, LOG_WARNING, "sigprocmask\n");
	fds[n_cpus].events = POLLIN;
	fds[n_cpus].fd = signalfd(-1, &mask, 0);
	if (fds[n_cpus].fd < 0) {
		log(TERM, LOG_WARNING, "signalfd\n");
		goto error;
	}

	log(TERM, LOG_INFO, "Listening to events for cpus 0 to %d\n", n_cpus - 1);
	if (pdata[0].ras->record_events) {
		if (ras_mc_event_opendb(pdata[0].cpu, pdata[0].ras))
			goto error;
#ifdef HAVE_NON_STANDARD
		if (ras_ns_add_vendor_tables(pdata[0].ras))
			log(TERM, LOG_ERR, "Can't add vendor table\n");
#endif
	}

	do {
		ready = poll(fds, (n_cpus + 1), -1);
		if (ready < 0)
			log(TERM, LOG_WARNING, "poll\n");

		/* check for the signal */
		if (fds[n_cpus].revents & POLLIN) {
			size = read(fds[n_cpus].fd, &fdsiginfo,
				    sizeof(struct signalfd_siginfo));
			if (size != sizeof(struct signalfd_siginfo))
				log(TERM, LOG_WARNING, "signalfd read\n");

			if (fdsiginfo.ssi_signo == SIGINT ||
			    fdsiginfo.ssi_signo == SIGTERM ||
			    fdsiginfo.ssi_signo == SIGHUP ||
			    fdsiginfo.ssi_signo == SIGQUIT) {
				log(TERM, LOG_INFO, "Received signal=%d\n",
				    fdsiginfo.ssi_signo);
				goto  cleanup;
			} else {
				log(TERM, LOG_INFO,
				    "Received unexpected signal=%d\n",
				    fdsiginfo.ssi_signo);
			}
		}

		count_nready = 0;
		for (i = 0; i < n_cpus; i++) {
			if (fds[i].revents & POLLERR) {
				if (!warnonce[i]) {
					log(TERM, LOG_INFO,
					    "Error on CPU %i\n", i);
					warnonce[i]++;
				}
			}
			if (!(fds[i].revents & POLLIN)) {
				count_nready++;
				continue;
			}
			size = read(fds[i].fd, page, pdata[i].ras->page_size);
			if (size < 0) {
				log(TERM, LOG_WARNING, "read\n");
				goto cleanup;
			} else if (size > 0) {
				kbuffer_load_subbuffer(kbuf, page);

				while ((data = kbuffer_read_event(kbuf, &time_stamp))) {
					if (kbuffer_curr_size(kbuf) < 0) {
						log(TERM, LOG_ERR, "invalid kbuf data, discard\n");
						break;
					}

					parse_ras_data(&pdata[i],
						       kbuf, data, time_stamp);

					/* increment to read next event */
					kbuffer_next_event(kbuf, NULL);
				}
			} else {
				count_nready++;
			}
		}
		/*
		 * If we enable fallback mode, it will always be used, as
		 * poll is still not working fine, IMHO
		 */
		if (count_nready == n_cpus) {
			/* Should only happen with legacy kernels */
			legacy_kernel = 1;
			break;
		}
	} while (1);

	/* poll() is not supported. We need to fallback to the old way */
	log(TERM, LOG_INFO,
	    "Old kernel detected. Stop listening and fall back to pthread way.\n");

cleanup:
	if (pdata[0].ras->record_events) {
#ifdef HAVE_NON_STANDARD
		ras_ns_finalize_vendor_tables();
#endif
		ras_mc_event_closedb(pdata[0].cpu, pdata[0].ras);
	}

error:
	kbuffer_free(kbuf);
	free(page);
	sigprocmask(SIG_UNBLOCK, &mask, NULL);

	for (i = 0; i < (n_cpus + 1); i++) {
		if (fds[i].fd > 0)
			close(fds[i].fd);
	}

	if (legacy_kernel)
		return LEGACY_KERNEL;

	return -EINVAL;
}

static int read_ras_event(int fd,
			  struct pthread_data *pdata,
			  struct kbuffer *kbuf,
			  void *page)
{
	int size;
	unsigned long long time_stamp;
	void *data;

	/*
	 * read() never blocks. We can't call poll() here, as it is
	 * not supported on kernels below 3.10. So, the better is to just
	 * sleep for a while, to avoid eating too much CPU here.
	 */
	do {
		size = read(fd, page, pdata->ras->page_size);
		if (size < 0) {
			log(TERM, LOG_WARNING, "read\n");
			return -EINVAL;
		} else if (size > 0) {
			kbuffer_load_subbuffer(kbuf, page);

			while ((data = kbuffer_read_event(kbuf, &time_stamp))) {
				parse_ras_data(pdata, kbuf, data, time_stamp);

				/* increment to read next event */
				kbuffer_next_event(kbuf, NULL);
			}
		} else {
			sleep(POLLING_TIME);
		}
	} while (1);
}

static void *handle_ras_events_cpu(void *priv)
{
	int fd;
	struct kbuffer *kbuf;
	void *page;
	char pipe_raw[PATH_MAX];
	struct pthread_data *pdata = priv;

	page = malloc(pdata->ras->page_size);
	if (!page) {
		log(TERM, LOG_ERR, "Can't allocate page\n");
		return NULL;
	}

	kbuf = kbuffer_alloc(KBUFFER_LSIZE_8, ENDIAN);
	if (!kbuf) {
		log(TERM, LOG_ERR, "Can't allocate kbuf");
		free(page);
		return NULL;
	}

	/* FIXME: use select to open for all CPUs */
	snprintf(pipe_raw, sizeof(pipe_raw),
		 "per_cpu/cpu%d/trace_pipe_raw",
		 pdata->cpu);

	fd = open_trace(pdata->ras, pipe_raw, O_RDONLY);
	if (fd < 0) {
		log(TERM, LOG_ERR, "Can't open trace_pipe_raw\n");
		kbuffer_free(kbuf);
		free(page);
		return NULL;
	}

	log(TERM, LOG_INFO, "Listening to events on cpu %d\n", pdata->cpu);
	if (pdata->ras->record_events) {
		pthread_mutex_lock(&pdata->ras->db_lock);
		if (ras_mc_event_opendb(pdata->cpu, pdata->ras)) {
			pthread_mutex_unlock(&pdata->ras->db_lock);
			log(TERM, LOG_ERR, "Can't open database\n");
			close(fd);
			kbuffer_free(kbuf);
			free(page);
			return 0;
		}
#ifdef HAVE_NON_STANDARD
		if (ras_ns_add_vendor_tables(pdata->ras))
			log(TERM, LOG_ERR, "Can't add vendor table\n");
#endif
		pthread_mutex_unlock(&pdata->ras->db_lock);
	}

	read_ras_event(fd, pdata, kbuf, page);

	if (pdata->ras->record_events) {
		pthread_mutex_lock(&pdata->ras->db_lock);
#ifdef HAVE_NON_STANDARD
		ras_ns_finalize_vendor_tables();
#endif
		ras_mc_event_closedb(pdata->cpu, pdata->ras);
		pthread_mutex_unlock(&pdata->ras->db_lock);
	}

	close(fd);
	kbuffer_free(kbuf);
	free(page);

	return NULL;
}

#define UPTIME "uptime"

static int select_tracing_timestamp(struct ras_events *ras)
{
	FILE *fp;
	int fd, rc;
	time_t uptime, now;
	size_t size;
	unsigned int j1;
	char buf[4096];

	/* Check if uptime is supported (kernel 3.10-rc1 or upper) */
	fd = open_trace(ras, "trace_clock", O_RDONLY);
	if (fd < 0) {
		log(TERM, LOG_ERR, "Can't open trace_clock\n");
		return -EINVAL;
	}
	size = read(fd, buf, sizeof(buf));
	close(fd);
	if (!size) {
		log(TERM, LOG_ERR, "trace_clock is empty!\n");
		return -EINVAL;
	}

	if (!strstr(buf, UPTIME)) {
		log(TERM, LOG_INFO, "Kernel doesn't support uptime clock\n");
		return 0;
	}

	/* Select uptime tracing */
	fd = open_trace(ras, "trace_clock", O_WRONLY);
	if (!fd) {
		log(TERM, LOG_ERR,
		    "Kernel didn't allow writing to trace_clock\n");
		return 0;
	}
	rc = write(fd, UPTIME, sizeof(UPTIME));
	close(fd);

	if (rc < 0) {
		log(TERM, LOG_ERR,
		    "Kernel didn't allow selecting uptime on trace_clock\n");
		return 0;
	}

	/* Reference uptime with localtime */
	fp = fopen("/proc/uptime", "r");
	if (!fp) {
		log(TERM, LOG_ERR,
		    "Couldn't read from /proc/uptime\n");
		return 0;
	}
	rc = fscanf(fp, "%zu.%u ", &uptime, &j1);
	fclose(fp);
	if (rc <= 0) {
		log(TERM, LOG_ERR, "Can't parse /proc/uptime!\n");
		return -EINVAL;
	}
	now = time(NULL);

	ras->use_uptime = 1;
	ras->uptime_diff = now - uptime;

	return 0;
}

#define EVENT_DISABLED	1

static int add_event_handler(struct ras_events *ras, struct tep_handle *pevent,
			     unsigned int page_size, char *group, char *event,
			     tep_event_handler_func func, char *filter_str, int id)
{
	int fd, rc;
	int size = 0;
	char *page, fname[MAX_PATH + 1];
	struct tep_event_filter *filter = NULL;

	snprintf(fname, sizeof(fname), "events/%s/%s/format", group, event);

	fd = open_trace(ras, fname, O_RDONLY);
	if (fd < 0) {
		if (fd == -ENOENT) {
			log(TERM, LOG_ERR,
			    "Feature %s:%s not supported on your system.\n",
			    group, event);
			return EVENT_DISABLED;
		}

		log(TERM, LOG_ERR, "Can't get %s:%s traces: %s\n",
		    group, event, strerror(-fd));

		return fd;
	}

	page = malloc(page_size);
	if (!page) {
		rc = -errno;
		log(TERM, LOG_ERR, "Can't allocate page to read %s:%s format\n",
		    group, event);
		close(fd);
		return rc;
	}

	do {
		rc = read(fd, page + size, page_size);
		if (rc < 0) {
			log(TERM, LOG_ERR, "Can't get arch page size\n");
			free(page);
			close(fd);
			return size;
		}
		size += rc;
	} while (rc > 0);
	close(fd);

	/* Registers the special event handlers */
	rc = tep_register_event_handler(pevent, -1, group, event, func, ras);
	if (rc == TEP_ERRNO__MEM_ALLOC_FAILED) {
		log(TERM, LOG_ERR, "Can't register event handler for %s:%s\n",
		    group, event);
		free(page);
		return -EINVAL;
	}

	rc = tep_parse_event(pevent, page, size, group);
	if (rc) {
		log(TERM, LOG_ERR, "Can't parse event %s:%s\n", group, event);
		free(page);
		return -EINVAL;
	}

	if (filter_str) {
		char error[255];

		filter = tep_filter_alloc(pevent);
		if (!filter) {
			log(TERM, LOG_ERR,
			    "Failed to allocate filter for %s/%s.\n", group, event);
			free(page);
			return -EINVAL;
		}
		rc = tep_filter_add_filter_str(filter, filter_str);
		if (rc < 0) {
			tep_filter_strerror(filter, rc, error, sizeof(error));
			log(TERM, LOG_ERR,
			    "Failed to install filter for %s/%s: %s\n", group, event, error);
			tep_filter_free(filter);
			free(page);
			return rc;
		}
	}

	ras->filters[id] = filter;

	if (is_disabled_event(group, event)) {
		log(ALL, LOG_INFO, "Disabled %s:%s tracing from config\n",
		    group, event);
		return EVENT_DISABLED;
	}

	/* Enable RAS events */
	rc = __toggle_ras_mc_event(ras, group, event, 1);
	free(page);
	if (rc < 0) {
		log(TERM, LOG_ERR, "Can't enable %s:%s tracing\n",
		    group, event);

		return -EINVAL;
	}

	setup_event_trigger(event);

	log(ALL, LOG_INFO, "Enabled event %s:%s\n", group, event);

	return 0;
}

int handle_ras_events(int record_events, int enable_ipmitool)
{
	int rc, page_size, i;
	int num_events = 0;
	unsigned int cpus;
	struct tep_handle *pevent = NULL;
	struct pthread_data *data = NULL;
	struct ras_events *ras = NULL;
#ifdef HAVE_DEVLINK
	char *filter_str = NULL;
#endif

	ras = calloc(1, sizeof(*ras));
	if (!ras) {
		log(TERM, LOG_ERR, "Can't allocate memory for ras struct\n");
		return -errno;
	}

	rc = get_tracing_dir(ras);
	if (rc < 0) {
		log(TERM, LOG_ERR, "Can't locate a mounted debugfs\n");
		goto err;
	}

	rc = select_tracing_timestamp(ras);
	if (rc < 0)
		log(TERM, LOG_ERR, "Can't select a timestamp for tracing. Using default\n");

	pevent = tep_alloc();
	if (!pevent) {
		log(TERM, LOG_ERR, "Can't allocate pevent\n");
		rc = -errno;
		goto err;
	}

	page_size = get_pagesize(ras, pevent);

	ras->pevent = pevent;
	ras->page_size = page_size;
	ras->record_events = record_events;

#ifdef HAVE_MEMORY_ROW_CE_PFA
	ras_row_account_init();
#endif

#ifdef HAVE_MEMORY_CE_PFA
	/* FIXME: enable memory isolation unconditionally */
	ras_page_account_init();
#endif

	rc = add_event_handler(ras, pevent, page_size, "ras", "mc_event",
			       ras_mc_event_handler, NULL, MC_EVENT);
	if (!rc)
		num_events++;
	else if (rc != EVENT_DISABLED)
		log(ALL, LOG_ERR, "Can't get traces from %s:%s\n",
		    "ras", "mc_event");

#ifdef HAVE_AER
	ras_aer_handler_init(enable_ipmitool);
	rc = add_event_handler(ras, pevent, page_size, "ras", "aer_event",
			       ras_aer_event_handler, NULL, AER_EVENT);
	if (!rc)
		num_events++;
	else if (rc != EVENT_DISABLED && rc != ENOENT)
		log(ALL, LOG_ERR, "Can't get traces from %s:%s\n",
		    "ras", "aer_event");
#endif

#ifdef HAVE_NON_STANDARD
	rc = add_event_handler(ras, pevent, page_size, "ras", "non_standard_event",
			       ras_non_standard_event_handler, NULL, NON_STANDARD_EVENT);
	if (!rc)
		num_events++;
	else if (rc != EVENT_DISABLED)
		log(ALL, LOG_ERR, "Can't get traces from %s:%s\n",
		    "ras", "non_standard_event");
#endif

#ifdef HAVE_ARM
	rc = add_event_handler(ras, pevent, page_size, "ras", "arm_event",
			       ras_arm_event_handler, NULL, ARM_EVENT);
	if (!rc)
		num_events++;
	else if (rc != EVENT_DISABLED)
		log(ALL, LOG_ERR, "Can't get traces from %s:%s\n",
		    "ras", "arm_event");
#endif

	cpus = get_num_cpus(ras);

#ifdef HAVE_CPU_FAULT_ISOLATION
	ras_cpu_isolation_init(cpus);
#endif

#ifdef HAVE_MCE
	rc = register_mce_handler(ras, cpus);
	if (rc && rc != -ENOENT)
		log(ALL, LOG_INFO, "Can't register mce handler\n");
	if (ras->mce_priv) {
		rc = add_event_handler(ras, pevent, page_size,
				       "mce", "mce_record",
				       ras_mce_event_handler, NULL, MCE_EVENT);
		if (!rc)
			num_events++;
	else
		log(ALL, LOG_ERR, "Can't get traces from %s:%s\n",
		    "mce", "mce_record");
	}
#endif

#ifdef HAVE_EXTLOG
	rc = add_event_handler(ras, pevent, page_size, "ras", "extlog_mem_event",
			       ras_extlog_mem_event_handler, NULL, EXTLOG_EVENT);
	if (!rc) {
		/* tell kernel we are listening, so don't printk to console */
		(void)open("/sys/kernel/debug/ras/daemon_active", 0);
		num_events++;
	} else if (rc != EVENT_DISABLED)
		log(ALL, LOG_ERR, "Can't get traces from %s:%s\n",
		    "ras", "extlog_mem_event");
#endif

#ifdef HAVE_DEVLINK
	rc = add_event_handler(ras, pevent, page_size, "net",
			       "net_dev_xmit_timeout",
			       ras_net_xmit_timeout_handler, NULL, DEVLINK_EVENT);
	if (!rc)
		filter_str = "devlink/devlink_health_report:msg=~\'TX timeout*\'";

	rc = add_event_handler(ras, pevent, page_size, "devlink",
			       "devlink_health_report",
			       ras_devlink_event_handler, filter_str, DEVLINK_EVENT);
	if (!rc)
		num_events++;
	else if (rc != EVENT_DISABLED)
		log(ALL, LOG_ERR, "Can't get traces from %s:%s\n",
		    "devlink", "devlink_health_report");
#endif

#ifdef HAVE_DISKERROR
#ifdef HAVE_BLK_RQ_ERROR
	rc = add_event_handler(ras, pevent, page_size, "block",
			       "block_rq_error", ras_diskerror_event_handler,
				NULL, DISKERROR_EVENT);
	if (!rc)
		num_events++;
	else if (rc != EVENT_DISABLED)
		log(ALL, LOG_ERR, "Can't get traces from %s:%s\n",
		    "block", "block_rq_error");
#else
	rc = filter_ras_mc_event(ras, "block", "block_rq_complete", "error != 0");
	if (!rc) {
		rc = add_event_handler(ras, pevent, page_size, "block",
				       "block_rq_complete", ras_diskerror_event_handler,
					NULL, DISKERROR_EVENT);
		if (!rc)
			num_events++;
		else if (rc != EVENT_DISABLED)
			log(ALL, LOG_ERR, "Can't get traces from %s:%s\n",
			    "block", "block_rq_complete");
	}
#endif
#endif

#ifdef HAVE_MEMORY_FAILURE
	rc = add_event_handler(ras, pevent, page_size, "ras", "memory_failure_event",
			       ras_memory_failure_event_handler, NULL, MF_EVENT);
	if (!rc)
		num_events++;
	else if (rc != EVENT_DISABLED)
		log(ALL, LOG_ERR, "Can't get traces from %s:%s\n",
		    "ras", "memory_failure_event");
#endif

#ifdef HAVE_CXL
	rc = add_event_handler(ras, pevent, page_size, "cxl", "cxl_poison",
			       ras_cxl_poison_event_handler, NULL, CXL_POISON_EVENT);
	if (!rc)
		num_events++;
	else if (rc != EVENT_DISABLED)
		log(ALL, LOG_ERR, "Can't get traces from %s:%s\n",
		    "cxl", "cxl_poison");

	rc = add_event_handler(ras, pevent, page_size, "cxl", "cxl_aer_uncorrectable_error",
			       ras_cxl_aer_ue_event_handler, NULL, CXL_AER_UE_EVENT);
	if (!rc)
		num_events++;
	else if (rc != EVENT_DISABLED)
		log(ALL, LOG_ERR, "Can't get traces from %s:%s\n",
		    "cxl", "cxl_aer_uncorrectable_error");

	rc = add_event_handler(ras, pevent, page_size, "cxl", "cxl_aer_correctable_error",
			       ras_cxl_aer_ce_event_handler, NULL, CXL_AER_CE_EVENT);
	if (!rc)
		num_events++;
	else if (rc != EVENT_DISABLED)
		log(ALL, LOG_ERR, "Can't get traces from %s:%s\n",
		    "cxl", "cxl_aer_correctable_error");

	rc = add_event_handler(ras, pevent, page_size, "cxl", "cxl_overflow",
			       ras_cxl_overflow_event_handler, NULL, CXL_OVERFLOW_EVENT);
	if (!rc)
		num_events++;
	else if (rc != EVENT_DISABLED)
		log(ALL, LOG_ERR, "Can't get traces from %s:%s\n",
		    "cxl", "cxl_overflow");

	rc = add_event_handler(ras, pevent, page_size, "cxl", "cxl_generic_event",
			       ras_cxl_generic_event_handler, NULL, CXL_GENERIC_EVENT);
	if (!rc)
		num_events++;
	else if (rc != EVENT_DISABLED)
		log(ALL, LOG_ERR, "Can't get traces from %s:%s\n",
		    "cxl", "cxl_generic_event");

	rc = add_event_handler(ras, pevent, page_size, "cxl", "cxl_general_media",
			       ras_cxl_general_media_event_handler, NULL, CXL_GENERAL_MEDIA_EVENT);
	if (!rc)
		num_events++;
	else if (rc != EVENT_DISABLED)
		log(ALL, LOG_ERR, "Can't get traces from %s:%s\n",
		    "cxl", "cxl_general_media");

	rc = add_event_handler(ras, pevent, page_size, "cxl", "cxl_dram",
			       ras_cxl_dram_event_handler, NULL, CXL_DRAM_EVENT);
	if (!rc)
		num_events++;
	else if (rc != EVENT_DISABLED)
		log(ALL, LOG_ERR, "Can't get traces from %s:%s\n",
		    "cxl", "cxl_dram");

	rc = add_event_handler(ras, pevent, page_size, "cxl", "cxl_memory_module",
			       ras_cxl_memory_module_event_handler, NULL, CXL_MEMORY_MODULE_EVENT);
	if (!rc)
		num_events++;
	else if (rc != EVENT_DISABLED)
		log(ALL, LOG_ERR, "Can't get traces from %s:%s\n",
		    "cxl", "memory_module");
#endif

	if (!num_events) {
		log(ALL, LOG_INFO,
		    "Failed to trace any supported RAS events. Aborting.\n");
		rc = -EINVAL;
		goto err;
	}

	data = calloc(cpus, sizeof(*data));
	if (!data)
		goto err;

	for (i = 0; i < cpus; i++) {
		data[i].ras = ras;
		data[i].cpu = i;
	}
	rc = read_ras_event_all_cpus(data, cpus);

	/* Poll doesn't work on this kernel. Fallback to pthread way */
	if (rc == LEGACY_KERNEL) {
		if (pthread_mutex_init(&ras->db_lock, NULL) != 0) {
			log(SYSLOG, LOG_INFO, "sqlite db lock init has failed\n");
			goto err;
		}

		log(SYSLOG, LOG_INFO,
		    "Opening one thread per cpu (%d threads)\n", cpus);
		for (i = 0; i < cpus; i++) {
			rc = pthread_create(&data[i].thread, NULL,
					    handle_ras_events_cpu,
					(void *)&data[i]);
			if (rc) {
				log(SYSLOG, LOG_INFO,
				    "Failed to create thread for cpu %d. Aborting.\n",
				i);
				while (--i)
					pthread_cancel(data[i].thread);

				pthread_mutex_destroy(&ras->db_lock);
				goto err;
			}
		}

		/* Wait for all threads to complete */
		for (i = 0; i < cpus; i++)
			pthread_join(data[i].thread, NULL);
		pthread_mutex_destroy(&ras->db_lock);
	}

	log(SYSLOG, LOG_INFO, "Huh! something got wrong. Aborting.\n");

err:
	if (data)
		free(data);

	if (pevent)
		tep_free(pevent);

	if (ras) {
		for (i = 0; i < NR_EVENTS; i++) {
			if (ras->filters[i])
				tep_filter_free(ras->filters[i]);
		}
		free(ras);
	}
#ifdef HAVE_CPU_FAULT_ISOLATION
	cpu_infos_free();
#endif

#ifdef HAVE_MEMORY_ROW_CE_PFA
	row_record_infos_free();
#endif
	return rc;
}
