/*
 * Copyright (C) 2013 Mauro Carvalho Chehab <mchehab@redhat.com>
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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
*/
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/poll.h>
#include "libtrace/kbuffer.h"
#include "libtrace/event-parse.h"
#include "ras-mc-event.h"
#include "ras-record.h"

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

#define DEBUGFS "/sys/kernel/debug/"

#define ENABLE_RAS_MC_EVENT  "ras:mc_event"
#define DISABLE_RAS_MC_EVENT "!" ENABLE_RAS_MC_EVENT

/*
 * Tracing enable/disable code
 */
int toggle_ras_mc_event(int enable)
{
	int fd, rc;

	/* Enable RAS events */
	fd = open(DEBUGFS "tracing/set_event", O_RDWR | O_APPEND);
	if (fd < 0) {
		perror("Open set_event");
		return errno;
	}
	if (enable)
		rc = write(fd, ENABLE_RAS_MC_EVENT,
			   sizeof(ENABLE_RAS_MC_EVENT));
	else
		rc = write(fd, DISABLE_RAS_MC_EVENT,
			   sizeof(DISABLE_RAS_MC_EVENT));
	if (rc < 0) {
		perror("can't write to set_event");
		close(fd);
		return rc;
	}
	close(fd);
	if (!rc) {
		fprintf(stderr, "nothing was written on set_event\n");
		return EIO;
	}

	if (enable)
		printf("RAS events enabled\n");
	else
		printf("RAS events disabled\n");

	return 0;
}

/*
 * Tracing read code
 */

/* Should match the code at Kernel's include/linux/edac.c */
enum hw_event_mc_err_type {
	HW_EVENT_ERR_CORRECTED,
	HW_EVENT_ERR_UNCORRECTED,
	HW_EVENT_ERR_FATAL,
	HW_EVENT_ERR_INFO,
};

static char *mc_event_error_type(unsigned long long err_type)
{
	switch (err_type) {
	case HW_EVENT_ERR_CORRECTED:
		return "Corrected";
	case HW_EVENT_ERR_UNCORRECTED:
		return "Uncorrected";
	case HW_EVENT_ERR_FATAL:
		return "Fatal";
	default:
	case HW_EVENT_ERR_INFO:
		return "Info";
	}
}

static int get_pagesize(struct pevent *pevent) {
	int fd, len, page_size = 4096;
	char buf[page_size];

	fd = open(DEBUGFS "tracing/events/header_page", O_RDONLY);
	if (fd < 0)
		return page_size;

	len = read(fd, buf, page_size);
	if (len <= 0)
		goto error;
	if (pevent_parse_header_page(pevent, buf, len, sizeof(long)))
		goto error;

	page_size = pevent->header_page_data_offset + pevent->header_page_data_size;

error:
	close(fd);
	return page_size;

}

static int ras_mc_event_handler(struct trace_seq *s,
				struct pevent_record *record,
				struct event_format *event, void *context)
{
	int len;
	unsigned long long val;
	struct ras_events *ras = context;
	struct timeval tv;
	struct tm *tm;
	struct ras_mc_event ev;
	char fmt[64];

	tv.tv_sec = record->ts / 1000000L;
	tv.tv_usec = record->ts % 1000000L;

	tm = localtime(&tv.tv_sec);
	if(tm) {
		strftime(fmt, sizeof(fmt), "%Y-%m-%d %H:%M:%S.%%06u %z", tm);
		snprintf(ev.timestamp, sizeof(ev.timestamp), fmt, tv.tv_usec);
	}

	/* Doesn't work... we need a hack here */
trace_seq_printf(s, "%s(%lld = %ld.%ld) ", ev.timestamp, record->ts, (long)tv.tv_sec, (long)tv.tv_usec);

	if (pevent_get_field_val(s,  event, "ev.error_count", record, &val, 0) < 0)
		return -1;
	ev.error_count = val;
	trace_seq_printf(s, "%d ", ev.error_count);

	if (pevent_get_field_val(s, event, "ev.error_type", record, &val, 1) < 0)
		return -1;
	ev.error_type = mc_event_error_type(val);
	trace_seq_puts(s, ev.error_type);
	if (ev.error_count > 1)
		trace_seq_puts(s, " errors:");
	else
		trace_seq_puts(s, " error:");

	ev.msg = pevent_get_field_raw(s, event, "ev.msg", record, &len, 1);
	if (!ev.msg)
		return -1;
	if (*ev.msg) {
		trace_seq_puts(s, " ");
		trace_seq_puts(s, ev.msg);
	}

	ev.label = pevent_get_field_raw(s, event, "ev.label", record, &len, 1);
	if (!ev.label)
		return -1;
	if (*ev.label) {
		trace_seq_puts(s, " on ");
		trace_seq_puts(s, ev.label);
	}

	trace_seq_puts(s, " (");
	if (pevent_get_field_val(s,  event, "ev.mc_index", record, &val, 0) < 0)
		return -1;
	ev.mc_index = val;
	trace_seq_printf(s, "mc: %d", ev.mc_index);

	if (pevent_get_field_val(s,  event, "ev.top_layer", record, &val, 0) < 0)
		return -1;
	ev.top_layer = (int) val;
	if (pevent_get_field_val(s,  event, "ev.middle_layer", record, &val, 0) < 0)
		return -1;
	ev.middle_layer = (int) val;
	if (pevent_get_field_val(s,  event, "ev.lower_layer", record, &val, 0) < 0)
		return -1;
	ev.lower_layer = (int) val;

	if (ev.top_layer == 255)
		ev.top_layer = -1;
	if (ev.middle_layer == 255)
		ev.middle_layer = -1;
	if (ev.lower_layer == 255)
		ev.lower_layer = -1;

	if (ev.top_layer >= 0 || ev.middle_layer >= 0 || ev.lower_layer >= 0) {
		if (ev.lower_layer >= 0)
			trace_seq_printf(s, " location: %d:%d:%d",
					ev.top_layer, ev.middle_layer, ev.lower_layer);
		else if (ev.middle_layer >= 0)
			trace_seq_printf(s, " location: %d:%d",
					 ev.top_layer, ev.middle_layer);
		else
			trace_seq_printf(s, " location: %d", ev.top_layer);
	}

	if (pevent_get_field_val(s,  event, "ev.address", record, &val, 0) < 0)
		return -1;
	ev.address = val;
	if (ev.address)
		trace_seq_printf(s, " ev.address: 0x%08llx", ev.address);

	if (pevent_get_field_val(s,  event, "ev.grain_bits", record, &val, 0) < 0)
		return -1;
	ev.grain = val;
	trace_seq_printf(s, " ev.grain: %lld", ev.grain);


	if (pevent_get_field_val(s,  event, "ev.syndrome", record, &val, 0) < 0)
		return -1;
	ev.syndrome = val;
	if (val)
		trace_seq_printf(s, " ev.syndrome: 0x%08llx", ev.syndrome);

	ev.driver_detail = pevent_get_field_raw(s, event, "ev.driver_detail", record,
					     &len, 1);
	if (!ev.driver_detail)
		return -1;
	if (*ev.driver_detail) {
		trace_seq_puts(s, " ");
		trace_seq_puts(s, ev.driver_detail);
	}
	trace_seq_puts(s, ")");

	/* Insert data into the SGBD */

	ras_store_mc_event(ras, &ev);

	return 0;
}

static void parse_ras_data(struct pthread_data *pdata, struct kbuffer *kbuf,
			   void *data, unsigned long long time_stamp)
{
	struct pevent_record record;
	struct trace_seq s;

	record.ts = time_stamp;
	record.size = kbuffer_event_size(kbuf);
	record.data = data;
	record.offset = kbuffer_curr_offset(kbuf);

	/* note offset is just offset in subbuffer */
	record.missed_events = kbuffer_missed_events(kbuf);
	record.record_size = kbuffer_curr_size(kbuf);

	trace_seq_init(&s);
	printf("cpu %02d:", pdata->cpu);
	fflush(stdout);
	pevent_print_event(pdata->ras->pevent, &s, &record);
	trace_seq_do_printf(&s);
	printf("\n");
}

static int read_ras_event(int fd,
			  struct pthread_data *pdata,
			  struct kbuffer *kbuf,
			  void *page)
{
	static int warn_sleep = 0;
	int ready;
	unsigned size;
	unsigned long long time_stamp;
	void *data;
	struct pollfd fds = {
		.fd = fd,
		.events = POLLIN | POLLPRI,
	};

	do {
		ready = poll(&fds, 1, 0);
		if (ready < 0) {
			perror("poll");
		}
		size = read(fd, page, pdata->ras->page_size);
		if (size < 0) {
			perror ("read");
			return -1;
		} else if (size > 0) {
			kbuffer_load_subbuffer(kbuf, page);

			while ((data = kbuffer_read_event(kbuf, &time_stamp))) {
				parse_ras_data(pdata, kbuf, data, time_stamp);

				/* increment to read next event */
				kbuffer_next_event(kbuf, NULL);
			}
		} else {
			/*
			 * Before Kernel 3.10, read() never blocks. So, we
			 * need to sleep for a while
			 */
			if (!warn_sleep) {
				printf("Old kernel: need to sleep\n");
				warn_sleep = 1;
			}
			sleep(POLLING_TIME);
		}
	} while (1);
}

static int get_num_cpus(void)
{
	int num_cpus = 0;

	DIR		*dir;
	struct dirent	*entry;

	dir = opendir(DEBUGFS "tracing/per_cpu/");
	if (!dir)
		return -1;

	for (entry = readdir(dir); entry; entry = readdir(dir)) {
		if (strstr(entry->d_name, "cpu"))
			num_cpus++;
	}
	closedir(dir);

	return num_cpus;
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
		perror("Can't allocate page");
		return NULL;
	}

	kbuf = kbuffer_alloc(KBUFFER_LSIZE_8, ENDIAN);
	if (!kbuf) {
		perror("Can't allocate kbuf");
		free(page);
		return NULL;
	}

	/* FIXME: use select to open for all CPUs */
	snprintf(pipe_raw, sizeof(pipe_raw),
		 DEBUGFS "tracing/per_cpu/cpu%d/trace_pipe_raw",
		 pdata->cpu);

	fd = open(pipe_raw, O_RDONLY);
	if (fd < 0) {
		perror("Can't open trace_pipe_raw");
		kbuffer_free(kbuf);
		free(page);
		return NULL;
	}

//	printf("Listening to events on cpu %d\n", pdata->cpu);

	read_ras_event(fd, pdata, kbuf, page);

	close(fd);
	kbuffer_free(kbuf);
	free(page);

	return NULL;
}

int handle_ras_events(int record_events)
{
	int rc, fd, size, page_size, i, cpus;
	struct pevent *pevent;
	struct pthread_data *data;
	struct ras_events *ras;
	void *page;

	/* Enable RAS events */
	rc = toggle_ras_mc_event(1);

	pevent = pevent_alloc();
	if (!pevent) {
		perror("Can't allocate pevent");
		return errno;
	}

	fd = open(DEBUGFS "tracing/events/ras/mc_event/format",
		  O_RDONLY);
	if (fd < 0) {
		perror("Open ras format");
		rc = errno;
		goto free_pevent;
	}

	page_size = get_pagesize(pevent);

	page = malloc(page_size);
	if (!page) {
		perror("Can't allocate page to read event format");
		rc = errno;
		close(fd);
		goto free_pevent;
	}

	size = read(fd, page, page_size);
	close(fd);
	if (size < 0) {
		rc = size;
		free(page);
		goto free_pevent;
	}

	ras = calloc(sizeof(*data), 1);
	if (!ras)
		goto free_pevent;
	ras->pevent = pevent;
	ras->page_size = page_size;

	if (record_events) {
		ras->db = ras_mc_event_opendb(ras);

		if (ras->db)
			printf("Recording events\n");
	}

	pevent_register_event_handler(pevent, -1, "ras", "mc_event",
				      ras_mc_event_handler, ras);

	rc = pevent_parse_event(pevent, page, size, "ras");
	free(page);
	if (rc)
		goto free_ras;

	cpus = get_num_cpus();
	data = calloc(sizeof(*data), cpus);
	if (!data)
		goto free_ras;

	printf("Opening one thread per cpu (%d threads)\n", cpus);
	for (i = 0; i < cpus; i++) {
		data[i].ras = ras;
		data[i].cpu = i;

		rc = pthread_create(&data[i].thread, NULL,
				    handle_ras_events_cpu,
				    (void *)&data[i]);
		if (rc) {
			while (--i)
				pthread_cancel(data[i].thread);
			goto free_threads;
		}
	}

	/* Wait for all threads to complete */
	for (i = 0; i < cpus; i++)
		pthread_join(data[i].thread, NULL);

free_threads:
	free(data);
free_ras:
	free(ras);

free_pevent:
	pevent_free(pevent);
	return rc;
}
