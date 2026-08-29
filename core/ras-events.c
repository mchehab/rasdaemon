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
#include <sys/queue.h>
#include <sys/types.h>
#include <traceevent/event-parse.h>
#include <traceevent/kbuffer.h>
#include <unistd.h>

#include "core/ras-events.h"
#include "core/ras-logger.h"

#define TOOL_NAME "rasdaemon"

/* Compatibility with libtraceevent versions that predate the host aliases. */
#ifndef KBUFFER_LSIZE_SAME_AS_HOST
#  if __SIZEOF_LONG__ == 8
#    define KBUFFER_LSIZE_SAME_AS_HOST KBUFFER_LSIZE_8
#  elif __SIZEOF_LONG__ == 4
#    define KBUFFER_LSIZE_SAME_AS_HOST KBUFFER_LSIZE_4
#  else
#    error "Unsupported long size"
#  endif
#endif

#ifndef KBUFFER_ENDIAN_SAME_AS_HOST
#  if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#    define KBUFFER_ENDIAN_SAME_AS_HOST KBUFFER_ENDIAN_LITTLE
#  elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#    define KBUFFER_ENDIAN_SAME_AS_HOST KBUFFER_ENDIAN_BIG
#  else
#    error "Unsupported byte order"
#  endif
#endif

/*
 * Polling time, if read() doesn't block. Currently, trace_pipe_raw never
 * blocks on read(). So, we need to sleep for a while, to avoid spending
 * too much CPU cycles. A fix for it is expected for 3.10.
 */
#define POLLING_TIME 3

/* Test for a little-endian machine */
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
	#define ENDIAN TEP_LITTLE_ENDIAN
#else
	#define ENDIAN TEP_BIG_ENDIAN
#endif

/**
 * var choices_disable - comma/space-separated events disabled by config
 */
char *choices_disable;
/**
 * var user_hz - userspace clock ticks per second
 */
long user_hz;

/**
 * struct ras_event_runtime - registry wrapper for an event descriptor
 * @entry: static event descriptor
 * @node: link in ras_event_handlers
 */
struct ras_event_runtime {
	const struct ras_event_entry *entry;
	LIST_ENTRY(ras_event_runtime) node;
};

LIST_HEAD(ras_event_list, ras_event_runtime);

/**
 * var ras_event_handlers - event descriptors in preparation order
 */
static struct ras_event_list ras_event_handlers =
	LIST_HEAD_INITIALIZER(ras_event_handlers);

/**
 * get_mountdir_by_type - find a mounted filesystem by type
 * @mount_type: filesystem type from /proc/mounts
 * @tracing_dir: destination for the mount path
 * @len: size of @tracing_dir
 *
 * Return:
 * * 0 - a matching mount was found and copied to @tracing_dir
 * * -ENOENT - no mount has @mount_type
 * * otherwise - a negative errno value from opening /proc/mounts
 */
static int get_mountdir_by_type(char *mount_type, char *tracing_dir, size_t len)
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

		if (!strcmp(type, mount_type)) {
			fclose(fp);
			strscpy(tracing_dir, dir, len - 1);
			return 0;
		}
	} while (1);

	fclose(fp);
	log(ALL, LOG_INFO, "Can't find mountdir for type: %s\n", mount_type);
	return -ENOENT;
}

/**
 * get_debugfs_dir - locate the debugfs mount
 * @tracing_dir: destination path
 * @len: destination size
 *
 * Return:
 * 0 on success or a negative errno value from get_mountdir_by_type().
 */
static int get_debugfs_dir(char *tracing_dir, size_t len)
{
	return get_mountdir_by_type("debugfs", tracing_dir, len);
}


/**
 * get_tracefs_dir - locate the tracefs mount
 * @tracing_dir: destination path
 * @len: destination size
 *
 * Return:
 * 0 on success or a negative errno value from get_mountdir_by_type().
 */
static int get_tracefs_dir(char *tracing_dir, size_t len)
{
	return get_mountdir_by_type("tracefs", tracing_dir, len);
}

/**
 * wait_access - wait for a tracefs node to appear
 * @path: node path
 * @ms: maximum wait in milliseconds
 *
 * Return:
 * * 0 - @path became accessible
 * * -1 - the timeout expired
 */
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

/**
 * open_trace - open a node relative to the tracing directory
 * @ras: initialized tracing context
 * @name: relative tracefs path
 * @flags: open(2) flags
 *
 * Return:
 * * nonnegative - an open file descriptor
 * * -E2BIG - the constructed path did not fit
 * * -1 - the trace node did not appear before the timeout
 * * otherwise - a negative errno value from open(2)
 */
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
	if (rc < 0)
		return -errno;

	return rc;
}

/**
 * get_tracing_dir - locate or create rasdaemon's tracing instance
 * @ras: context receiving the path
 *
 * Prefers tracefs and falls back to debugfs/tracing. When trace instances are
 * supported, creates or reuses the ``rasdaemon`` instance.
 *
 * Return:
 * * 0 - @ras->tracing contains the usable tracing directory
 * * -E2BIG - the constructed path did not fit
 * * -EINVAL - the directory could not be opened or the instance created
 * * otherwise - a negative mount-discovery error
 */
static int get_tracing_dir(struct ras_events *ras)
{
	char		fname[MAX_PATH + 1];
	char		debugfs[MAX_PATH + 1];
	int		rc, has_instances = 0;
	DIR		*dir;
	struct dirent	*entry;

	rc = get_tracefs_dir(fname, sizeof(fname));
	if (rc < 0)
	{
		/* check under deprecated debugfs location */
		rc = get_debugfs_dir(debugfs, sizeof(debugfs));
		if (rc < 0)
			return rc;

		rc = strscpy(fname, debugfs, sizeof(fname));
		if (rc < 0)
			return rc;
		rc = strscat(fname, "/tracing", sizeof(fname));
		if (rc < 0)
			return rc;
	}

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

	strscpy(ras->tracing, fname, sizeof(ras->tracing));
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

/**
 * is_disabled_event - check the configured trace-event deny list
 * @group: trace subsystem
 * @event: trace event name
 *
 * Return:
 * true if the exact ``group:event`` name is disabled.
 */
static bool is_disabled_event(const char *group, const char *event)
{
	char ras_event_name[MAX_PATH + 1];
	const char *choice;
	size_t name_len;

	snprintf(ras_event_name, sizeof(ras_event_name), "%s:%s",
		 group, event);

	name_len = strlen(ras_event_name);
	choice = choices_disable;
	while (choice && *choice) {
		const char *end;

		choice += strspn(choice, ", \t");
		if (!*choice)
			break;
		end = choice + strcspn(choice, ", \t");
		if ((size_t)(end - choice) == name_len &&
		    !strncmp(choice, ras_event_name, name_len))
			return true;
		choice = end;
	}
	return false;
}

#ifdef HAVE_UNITTEST
/**
 * ras_events_test_is_disabled - unit-test access to is_disabled_event()
 * @group: trace subsystem
 * @event: trace event name
 *
 * Return:
 * true if disabled by choices_disable.
 */
bool ras_events_test_is_disabled(const char *group, const char *event)
{
	return is_disabled_event(group, event);
}
#endif

/*
 * Tracing enable/disable code
 */
/**
 * __toggle_ras_mc_event - enable or disable one kernel trace event
 * @ras: tracing context
 * @group: trace subsystem
 * @event: trace event name
 * @enable: nonzero to enable unless configured disabled
 *
 * Return:
 * * 0 - the event state was written successfully
 * * -EIO - write(2) reported that no bytes were written
 * * otherwise - a negative/open failure or the write(2) failure value
 */
static int __toggle_ras_mc_event(struct ras_events *ras,
				 const char *group, const char *event, int enable)
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

/**
 * toggle_ras_mc_event - toggle every registered RAS trace event
 * @enable: nonzero to enable events, zero to disable them
 *
 * Return:
 * * 0 - every registered event was toggled
 * * -EINVAL - tracing setup or at least one event toggle failed
 * * otherwise - the negative allocation errno from calloc(3)
 */
int toggle_ras_mc_event(int enable)
{
	struct ras_event_runtime *event;
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

	LIST_FOREACH(event, &ras_event_handlers, node)
		rc |= __toggle_ras_mc_event(ras, event->entry->group,
					    event->entry->event, enable);

free_ras:
	free(ras);
	if (rc)
		return -EINVAL;

	return 0;
}

/*
 * Set kernel filter. libtrace doesn't provide an API for setting filters
 * in kernel, we have to implement it here.
 */
/**
 * filter_ras_mc_event - install a kernel-side trace-event filter
 * @ras: tracing context
 * @group: trace subsystem
 * @event: trace event name
 * @filter_str: kernel filter expression
 *
 * Return:
 * * 0 - the filter was written successfully
 * * -EIO - write(2) reported that no bytes were written
 * * otherwise - a negative/open failure or the write(2) failure value
 */
static int filter_ras_mc_event(struct ras_events *ras, const char *group,
			       const char *event,
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

/**
 * ras_event_filter - install a kernel filter for a registered event
 * @ras: tracing context
 * @group: trace subsystem
 * @event: trace event name
 * @filter: kernel filter expression
 *
 * Return:
 * the result of installing @filter through filter_ras_mc_event().
 */
int ras_event_filter(struct ras_events *ras, const char *group,
		     const char *event, const char *filter)
{
	return filter_ras_mc_event(ras, group, event, filter);
}

/*
 * Tracing read code
 */

/**
 * get_pagesize - parse the trace ring-buffer page header
 * @ras: tracing context
 * @pevent: event parser receiving header metadata
 *
 * Return:
 * 4096, after parsing the page header when it is available.
 */
static int get_pagesize(struct ras_events *ras, struct tep_handle *pevent)
{
	int fd, len, page_size = 4096;
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

/**
 * parse_ras_data - dispatch one raw ring-buffer record
 * @pdata: reader state
 * @kbuf: loaded kernel ring buffer
 * @data: current record payload
 * @time_stamp: record timestamp
 */
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
	tep_set_file_bigendian(pdata->ras->pevent, ENDIAN);
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

/**
 * get_num_cpus - obtain the number of online logical CPUs
 * @ras: tracing context (unused)
 *
 * Return:
 * the positive number of online logical CPUs. Failure triggers an assertion.
 */
static int get_num_cpus(struct ras_events *ras)
{
	int cpus;

	cpus = sysconf(_SC_NPROCESSORS_ONLN);
	assert(cpus > 0);
	return cpus;
}

/**
 * set_buffer_percent - configure the trace-buffer poll wake threshold
 * @ras: tracing context
 * @percent: percentage written to tracefs
 *
 * Return:
 * * 0 - the percentage was written
 * * -EINVAL - the node could not be opened or written
 */
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

/**
 * read_ras_event_all_cpus - poll all per-CPU trace pipes in one thread
 * @pdata: array containing at least @n_cpus reader contexts
 * @n_cpus: number of CPU pipes
 *
 * SIGINT, SIGTERM, SIGHUP, and SIGQUIT are blocked while polling and restored
 * before return. All file descriptors and buffers are released on every path.
 *
 * Return:
 * * @LEGACY_KERNEL - polling appears unsupported and callers should fall back
 * * -ENOMEM - a page or kernel-buffer decoder could not be allocated
 * * -EINVAL - signal-driven shutdown or any other polling/read failure
 */
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
	sigset_t mask, oldmask;
	int warnonce[n_cpus];
	char pipe_raw[PATH_MAX];
	int legacy_kernel = 0;

	memset(&warnonce, 0, sizeof(warnonce));

	page = malloc(pdata[0].ras->page_size);
	if (!page) {
		log(TERM, LOG_ERR, "Can't allocate page\n");
		return -ENOMEM;
	}

	kbuf = kbuffer_alloc(KBUFFER_LSIZE_SAME_AS_HOST, KBUFFER_ENDIAN_SAME_AS_HOST);
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
	if (sigprocmask(SIG_BLOCK, &mask, &oldmask) == -1) {
		log(TERM, LOG_WARNING, "sigprocmask\n");
		goto error;
	}
	fds[n_cpus].events = POLLIN;
	fds[n_cpus].fd = signalfd(-1, &mask, 0);
	if (fds[n_cpus].fd < 0) {
		log(TERM, LOG_WARNING, "signalfd\n");
		goto cleanup;
	}

	log(TERM, LOG_INFO, "Listening to events for cpus 0 to %d\n", n_cpus - 1);
	do {
		ready = poll(fds, (n_cpus + 1), -1);
		if (ready < 0) {
			if (errno == EINTR)
				continue;
			log(TERM, LOG_WARNING, "poll\n");
			goto cleanup;
		}

		/* check for the signal */
		if (fds[n_cpus].revents & (POLLERR | POLLHUP | POLLNVAL)) {
			log(TERM, LOG_WARNING, "signalfd poll\n");
			goto cleanup;
		}
		if (fds[n_cpus].revents & POLLIN) {
			size = read(fds[n_cpus].fd, &fdsiginfo,
				    sizeof(fdsiginfo));
			if (size != sizeof(fdsiginfo)) {
				log(TERM, LOG_WARNING, "signalfd read\n");
				goto cleanup;
			}

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
	sigprocmask(SIG_SETMASK, &oldmask, NULL);
error:
	kbuffer_free(kbuf);
	free(page);

	for (i = 0; i < (n_cpus + 1); i++) {
		if (fds[i].fd >= 0)
			close(fds[i].fd);
	}

	if (legacy_kernel)
		return LEGACY_KERNEL;

	return -EINVAL;
}

/**
 * read_ras_event - read one legacy per-CPU trace pipe indefinitely
 * @fd: trace_pipe_raw descriptor
 * @pdata: CPU reader state
 * @kbuf: reusable kernel-buffer decoder
 * @page: reusable page-sized input buffer
 *
 * Cancellation is disabled while the shared database lock is held.
 *
 * Return:
 * -EINVAL on read failure; otherwise the loop does not return.
 */
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
				int oldstate;

				/*
				 * Legacy kernels use one reader per CPU.  Event handlers
				 * share database connections and prepared statements, so a
				 * complete callback must be atomic with respect to its peers.
				 */
				pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldstate);
				pthread_mutex_lock(&pdata->ras->db_lock);
				parse_ras_data(pdata, kbuf, data, time_stamp);
				pthread_mutex_unlock(&pdata->ras->db_lock);
				pthread_setcancelstate(oldstate, NULL);

				/* increment to read next event */
				kbuffer_next_event(kbuf, NULL);
			}
		} else {
			sleep(POLLING_TIME);
		}
	} while (1);
}

/**
 * struct reader_cleanup - cancellation-owned per-CPU reader resources
 * @pdata: reader state
 * @kbuf: kernel-buffer decoder
 * @page: raw input allocation
 * @fd: trace pipe descriptor
 */
struct reader_cleanup {
	struct pthread_data *pdata;
	struct kbuffer *kbuf;
	void *page;
	int fd;
};

/**
 * cleanup_ras_events_cpu - pthread cleanup handler for a CPU reader
 * @arg: struct reader_cleanup pointer
 */
static void cleanup_ras_events_cpu(void *arg)
{
	struct reader_cleanup *cleanup = arg;
	int oldstate;

	/* Cleanup itself must not be interrupted while owning the mutex. */
	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldstate);
	if (cleanup->fd >= 0)
		close(cleanup->fd);
	if (cleanup->kbuf)
		kbuffer_free(cleanup->kbuf);
	free(cleanup->page);
	pthread_setcancelstate(oldstate, NULL);
}

/**
 * handle_ras_events_cpu - legacy reader thread entry point
 * @priv: struct pthread_data pointer
 *
 * Return:
 * always NULL after its cleanup handler releases reader resources.
 */
static void *handle_ras_events_cpu(void *priv)
{
	char pipe_raw[PATH_MAX];
	struct pthread_data *pdata = priv;
	struct reader_cleanup cleanup = {
		.pdata = pdata,
		.fd = -1,
	};

	pthread_cleanup_push(cleanup_ras_events_cpu, &cleanup);
	cleanup.page = malloc(pdata->ras->page_size);
	if (!cleanup.page) {
		log(TERM, LOG_ERR, "Can't allocate page\n");
		goto out;
	}

	cleanup.kbuf = kbuffer_alloc(KBUFFER_LSIZE_SAME_AS_HOST, KBUFFER_ENDIAN_SAME_AS_HOST);
	if (!cleanup.kbuf) {
		log(TERM, LOG_ERR, "Can't allocate kbuf");
		goto out;
	}

	/* FIXME: use select to open for all CPUs */
	snprintf(pipe_raw, sizeof(pipe_raw),
		 "per_cpu/cpu%d/trace_pipe_raw",
		 pdata->cpu);

	cleanup.fd = open_trace(pdata->ras, pipe_raw, O_RDONLY);
	if (cleanup.fd < 0) {
		log(TERM, LOG_ERR, "Can't open trace_pipe_raw\n");
		goto out;
	}

	log(TERM, LOG_INFO, "Listening to events on cpu %d\n", pdata->cpu);
	read_ras_event(cleanup.fd, pdata, cleanup.kbuf, cleanup.page);

out:
	pthread_cleanup_pop(1);
	return NULL;
}

#define UPTIME "uptime"

/**
 * select_tracing_timestamp - prefer the trace uptime clock
 * @ras: context receiving clock-selection state
 *
 * Unsupported or unwritable uptime clocks are nonfatal and retain the kernel
 * default. /proc/uptime is used to compute the wall-clock offset.
 *
 * Return:
 * * 0 - uptime was selected or the kernel clock was retained as a fallback
 * * -EINVAL - trace_clock could not be opened/read or clock data was malformed
 */
static int select_tracing_timestamp(struct ras_events *ras)
{
	FILE *fp;
	int fd, rc;
	size_t uptime;
	time_t now;
	ssize_t size;
	unsigned int j1;
	char buf[4096];

	/* Check if uptime is supported (kernel 3.10-rc1 or upper) */
	fd = open_trace(ras, "trace_clock", O_RDONLY);
	if (fd < 0) {
		log(TERM, LOG_ERR, "Can't open trace_clock\n");
		return -EINVAL;
	}
	size = read(fd, buf, sizeof(buf) - 1);
	close(fd);
	if (size < 0) {
		log(TERM, LOG_ERR, "Can't read trace_clock\n");
		return -EINVAL;
	}
	if (!size) {
		log(TERM, LOG_ERR, "trace_clock is empty!\n");
		return -EINVAL;
	}
	buf[size] = '\0';

	if (!strstr(buf, UPTIME)) {
		log(TERM, LOG_INFO, "Kernel doesn't support uptime clock\n");
		return 0;
	}

	/* Select uptime tracing */
	fd = open_trace(ras, "trace_clock", O_WRONLY);
	if (fd < 0) {
		log(TERM, LOG_ERR,
		    "Kernel didn't allow writing to trace_clock\n");
		return 0;
	}
	size = write(fd, UPTIME, sizeof(UPTIME) - 1);
	close(fd);

	if (size != sizeof(UPTIME) - 1) {
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
	if (rc != 2) {
		log(TERM, LOG_ERR, "Can't parse /proc/uptime!\n");
		return -EINVAL;
	}
	now = time(NULL);

	ras->use_uptime = 1;
	ras->uptime_diff = now - (time_t)uptime;

	return 0;
}

/**
 * check_event_exist - test for an event directory in tracefs
 * @ras: tracing context
 * @group: trace subsystem
 * @event: trace event name
 *
 * Return:
 * true when the directory exists.
 */
static bool check_event_exist(struct ras_events *ras, const char *group,
			      const char *event)
{
	char fname[MAX_PATH + 256];

	snprintf(fname, sizeof(fname), "%s/events/%s/%s",
		 ras->tracing, group, event);
	if (access(fname, F_OK) == 0)
		return true;

	return false;
}

#define EVENT_DISABLED	1

/**
 * ras_events_unregister - release event-registry wrappers at process exit
 */
static void ras_events_unregister(void)
{
	struct ras_event_runtime *event;

	while ((event = LIST_FIRST(&ras_event_handlers))) {
		LIST_REMOVE(event, node);
		free(event);
	}
}

/**
 * ras_event_record - invoke the recorder registered for an event type
 * @ras: event/database context
 * @event_id: enum ras_event_id
 * @data: concrete event payload
 *
 * Return:
 * * 0 - a matching event has no recorder or its recorder succeeded
 * * -EINVAL - @ras, @data, or @event_id is invalid
 * * -ENOENT - no registered descriptor has @event_id
 * * otherwise - the matching recorder's error
 */
int ras_event_record(struct ras_events *ras, int event_id, void *data)
{
	struct ras_event_runtime *event;
	bool found = false;

	if (!ras || !data || event_id < 0 || event_id >= NR_EVENTS)
		return -EINVAL;

	LIST_FOREACH(event, &ras_event_handlers, node) {
		if (event->entry->id != event_id)
			continue;

		found = true;
		if (event->entry->record)
			return event->entry->record(ras, data);
	}

	return found ? 0 : -ENOENT;
}

#ifdef HAVE_UNITTEST
/**
 * ras_event_test_find - locate an event descriptor for unit tests
 * @group: trace subsystem
 * @name: trace event name
 *
 * Return:
 * registry-owned descriptor or NULL.
 */
static const struct ras_event_entry *ras_event_test_find(const char *group,
							 const char *name)
{
	struct ras_event_runtime *event;

	LIST_FOREACH(event, &ras_event_handlers, node)
		if (!strcmp(group, event->entry->group) &&
		    !strcmp(name, event->entry->event))
			return event->entry;
	return NULL;
}

/**
 * ras_event_test_handler - resolve a registered trace callback for tests
 * @group: trace subsystem
 * @event: trace event name
 *
 * Return:
 * handler callback or NULL when not registered.
 */
tep_event_handler_func ras_event_test_handler(const char *group,
					      const char *event)
{
	const struct ras_event_entry *entry = ras_event_test_find(group, event);

	return entry ? entry->handler : NULL;
}

#endif

/**
 * ras_event_register - register a static trace-event descriptor
 * @entry: descriptor with process lifetime
 *
 * Entries are sorted by @ras_event_entry.order, group, and event. Registration
 * is constructor-safe but not thread-safe. Associated tests are registered as
 * part of the same operation.
 *
 * Return:
 * * 0 - the event and any associated test were registered
 * * -EINVAL - @entry or one of its required fields is invalid
 * * -EEXIST - its trace pair or associated test callback is already registered
 * * -ENOMEM - wrapper allocation or exit-handler registration failed
 * * otherwise - the associated test-registration error
 */
int ras_event_register(const struct ras_event_entry *entry)
{
	struct ras_event_runtime *event, *new, *prev = NULL;
	static bool cleanup_registered;
	int cmp;

	if (!entry || !entry->group || !entry->event || !entry->handler ||
	    entry->id < 0 || entry->id >= NR_EVENTS)
		return -EINVAL;

	LIST_FOREACH(event, &ras_event_handlers, node) {
		if (!strcmp(entry->group, event->entry->group) &&
		    !strcmp(entry->event, event->entry->event))
			return -EEXIST;

		cmp = entry->order < event->entry->order ? -1 :
		      entry->order > event->entry->order;
		if (!cmp)
			cmp = strcmp(entry->group, event->entry->group);
		if (!cmp)
			cmp = strcmp(entry->event, event->entry->event);
		if (cmp < 0)
			break;
		prev = event;
	}

	new = calloc(1, sizeof(*new));
	if (!new)
		return -ENOMEM;
	new->entry = entry;

	if (event)
		LIST_INSERT_BEFORE(event, new, node);
	else if (prev)
		LIST_INSERT_AFTER(prev, new, node);
	else
		LIST_INSERT_HEAD(&ras_event_handlers, new, node);

	if (!cleanup_registered) {
		if (atexit(ras_events_unregister)) {
			LIST_REMOVE(new, node);
			free(new);
			return -ENOMEM;
		}
		cleanup_registered = true;
	}

#ifdef HAVE_UNITTEST
	if (entry->test) {
		cmp = module_test_register(entry->test_group, entry->test,
					   entry->test_priority);
		if (cmp) {
			LIST_REMOVE(new, node);
			free(new);
			return cmp;
		}
	}
#endif

	return 0;
}

/**
 * add_event_handler - parse, register, filter, and enable one trace event
 * @ras: tracing context
 * @pevent: event parser
 * @page_size: initial format-read allocation size
 * @group: trace subsystem
 * @event: trace event name
 * @func: libtraceevent callback
 * @filter_str: optional userspace filter expression
 * @id: enum ras_event_id used to store the allocated filter
 *
 * Return:
 * * 0 - the handler was registered and its trace event enabled
 * * @EVENT_DISABLED - its format node is absent or it is configured off
 * * -EOVERFLOW - the dynamically grown format buffer would overflow
 * * otherwise - a negative discovery, I/O, parsing, filter, or enable error
 *
 * A filter stored in @ras is freed by ras_events_cleanup().
 */
static int add_event_handler(struct ras_events *ras,
			     struct tep_handle *pevent,
			     unsigned int page_size, const char *group,
			     const char *event, tep_event_handler_func func,
			     const char *filter_str, int id)
{
	int fd, rc;
	int size = 0;
	char *page, fname[MAX_PATH + 1];
	struct tep_event_filter *filter = NULL;

	if (!check_event_exist(ras, group, event)) {
		log(ALL, LOG_WARNING, "%s:%s event not exist\n",
		    group, event);
		return -EINVAL;
	}

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
		if (size > 0) {
			char *new_page;

			if ((size_t)size > SIZE_MAX - page_size) {
				free(page);
				close(fd);
				return -EOVERFLOW;
			}
			new_page = realloc(page, page_size + (size_t)size);
			if (!new_page) {
				rc = -errno;
				log(TERM, LOG_ERR,
				    "Can't reallocate page to read %s:%s format\n",
				    group, event);
				free(page);
				close(fd);
				return rc;
			}
			page = new_page;
		}
		rc = read(fd, page + size, page_size);
		if (rc < 0) {
			log(TERM, LOG_ERR, "Can't get arch page size\n");
			free(page);
			close(fd);
			return rc;
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
			    "Failed to allocate filter for %s/%s.\n",
			    group, event);
			free(page);
			return -EINVAL;
		}
		rc = tep_filter_add_filter_str(filter, filter_str);
		if (rc < 0) {
			tep_filter_strerror(filter, rc, error, sizeof(error));
			log(TERM, LOG_ERR,
			    "Failed to install filter for %s/%s: %s\n",
			    group, event, error);
			tep_filter_free(filter);
			free(page);
			return rc;
		}
	}

	ras->filters[id] = filter;

	if (is_disabled_event(group, event)) {
		log(ALL, LOG_INFO, "Disabled %s:%s tracing from config\n",
		    group, event);
		free(page);
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

	log(ALL, LOG_INFO, "Enabled event %s:%s\n", group, event);

	return 0;
}

/**
 * ras_events_prepare - initialize tracing and all registered event handlers
 * @ras: zero-initialized process context
 * @record_events: enable database recording consumers
 * @enable_ipmitool: enable IPMI reporting
 *
 * The caller owns @ras and must call ras_events_cleanup() after a successful
 * call, including if a later database/module initialization step fails.
 * Individual unsupported events are logged and skipped.
 *
 * Return:
 * * 0 - basic preparation completed; unsupported individual events were skipped
 * * otherwise - a negative tracing-setup or parser-allocation error
 */
int ras_events_prepare(struct ras_events *ras, int record_events,
		       int enable_ipmitool)
{
	struct ras_event_runtime *event;
	struct tep_handle *pevent;
	int rc;

	if (!ras)
		return -EINVAL;

	rc = get_tracing_dir(ras);
	if (rc < 0) {
		log(TERM, LOG_ERR, "Can't locate a mounted debugfs\n");
		return rc;
	}

	rc = select_tracing_timestamp(ras);
	if (rc < 0)
		log(TERM, LOG_ERR,
		    "Can't select a timestamp for tracing. Using default\n");

	pevent = tep_alloc();
	if (!pevent)
		return -errno;

	ras->pevent = pevent;
	ras->page_size = get_pagesize(ras, pevent);
	ras->record_events = record_events;
	ras->enable_ipmitool = enable_ipmitool;
	ras->daemon_active_fd = -1;
	ras->num_events = 0;

	LIST_FOREACH(event, &ras_event_handlers, node) {
		const struct ras_event_entry *entry = event->entry;
		const char *filter = entry->filter;

		if (entry->prepare && entry->prepare(ras))
			continue;
		if (entry->filter_cb)
			filter = entry->filter_cb(ras);

		rc = add_event_handler(ras, pevent, ras->page_size,
				       entry->group, entry->event, entry->handler,
				       filter, entry->id);
		if (!rc) {
			ras->num_events++;
			if (entry->trigger_setup)
				entry->trigger_setup();
			if (entry->enabled)
				entry->enabled(ras);
			continue;
		}
		if (rc != EVENT_DISABLED)
			log(ALL, LOG_ERR, "Can't get traces from %s:%s\n",
			    entry->group, entry->event);
	}

	return 0;
}

/**
 * ras_events_cleanup - release resources allocated during event preparation
 * @ras: process context, or NULL
 *
 * The operation is idempotent after initialization and clears owned pointers.
 * It does not free @ras or clean module-owned resources.
 */
void ras_events_cleanup(struct ras_events *ras)
{
	int i;

	if (!ras)
		return;

	if (ras->daemon_active_fd >= 0) {
		close(ras->daemon_active_fd);
		ras->daemon_active_fd = -1;
	}

	for (i = 0; i < NR_EVENTS; i++) {
		if (!ras->filters[i])
			continue;
		tep_filter_free(ras->filters[i]);
		ras->filters[i] = NULL;
	}

	if (ras->pevent) {
		tep_free(ras->pevent);
		ras->pevent = NULL;
	}
	ras->num_events = 0;
}

/**
 * handle_ras_events - run the trace reader until shutdown or failure
 * @ras: successfully prepared event context
 *
 * Modern kernels use a single polling reader. Kernels without working poll
 * support fall back to one cancellable thread per CPU; handler/database access
 * is serialized in that mode. This function always calls ras_events_cleanup()
 * before returning.
 *
 * Return:
 * * -EINVAL - @ras is invalid, no events were enabled, or polling stopped
 * * -ENOMEM - per-CPU reader state could not be allocated
 * * @LEGACY_KERNEL - all fallback reader threads exited without setup failure
 * * otherwise - a negated pthread initialization or creation error
 */
int handle_ras_events(struct ras_events *ras)
{
	int rc, i;
	int num_events;
	unsigned int cpus;
	struct pthread_data *data = NULL;

	if (!ras || !ras->pevent)
		return -EINVAL;

	num_events = ras->num_events;
	cpus = get_num_cpus(ras);

	if (!num_events) {
		log(ALL, LOG_INFO,
		    "Failed to trace any supported RAS events. Aborting.\n");
		rc = -EINVAL;
		goto err;
	}

	data = calloc(cpus, sizeof(*data));
	if (!data) {
		rc = -ENOMEM;
		goto err;
	}

	for (i = 0; i < cpus; i++) {
		data[i].ras = ras;
		data[i].cpu = i;
	}
	rc = read_ras_event_all_cpus(data, cpus);

	/* Poll doesn't work on this kernel. Fallback to pthread way */
	if (rc == LEGACY_KERNEL) {
		rc = pthread_mutex_init(&ras->db_lock, NULL);
		if (rc) {
			log(SYSLOG, LOG_INFO, "SQL DB lock init has failed\n");
			rc = -rc;
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
				int started = i;

				while (i-- > 0)
					pthread_cancel(data[i].thread);
				for (i = 0; i < started; i++)
					pthread_join(data[i].thread, NULL);
				pthread_mutex_destroy(&ras->db_lock);
				rc = -rc;
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
	free(data);
	ras_events_cleanup(ras);
	return rc;
}
