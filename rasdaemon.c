/*
 * Copyright © 2013 Mauro Carvalho Chehab <mchehab@redhat.com>
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of Red Hat
 * not be used in advertising or publicity pertaining to distribution
 * of the software without specific, written prior permission.  Red
 * Hat makes no representations about the suitability of this software
 * for any purpose.  It is provided "as is" without express or implied
 * warranty.
 *
 * THE AUTHORS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN
 * NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <argp.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/poll.h>
#include "libtrace/kbuffer.h"
#include "libtrace/event-parse.h"

/*
 * BuildRequires: sqlite-devel
 */

#define SQLITE_RAS_DB	"ras-mc_event.db"
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

struct ras_events {
	struct pevent	*pevent;
	int		page_size;
	sqlite3		*db;
	sqlite3_stmt	*stmt;
};

struct pthread_data {
	pthread_t		thread;
	struct pevent		*pevent;
	struct ras_events	*ras;
	int			cpu;
};

/*
 * Tracing enable/disable code
 */
static int toggle_ras_mc_event(int enable)
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

const char *mc_event_db = " mc_event ";
const char *mc_event_db_create_fields = "("
		"id INTEGER PRIMARY KEY"
		", timestamp TEXT"
		", err_count INTEGER"
		", err_type TEXT"
		", err_msg TEXT"		/* 5 */
		", label TEXT"
		", mc INTEGER"
		", top_layer INTEGER"
		", middle_layer INTEGER"
		", lower_layer INTEGER"		/* 10 */
		", address INTEGER"
		", grain INTEGER"
		", syndrome INTEGER"
		", driver_detail TEXT"		/* 14 */
	")";

const char *mc_event_db_fields = "("
		"id"
		", timestamp"
		", err_count"
		", err_type"
		", err_msg"			/* 5 */
		", label"
		", mc"
		", top_layer"
		", middle_layer"
		", lower_layer"			/* 10 */
		", address"
		", grain"
		", syndrome"
		", driver_detail"		/* 14 */
	")";

#define NUM_MC_EVENT_DB_VALUES	14

const char *createdb = "CREATE TABLE IF NOT EXISTS";
const char *insertdb = "INSERT INTO";
const char *valuesdb = " VALUES ";

static sqlite3 *ras_mc_event_opendb(struct ras_events *ras)
{
	int rc, i;
	sqlite3 *db;
	char sql[1024];

	rc = sqlite3_initialize();
	if (rc != SQLITE_OK) {
		printf("Failed to initialize sqlite: error = %d\n", rc);
		return NULL;
	}

	rc = sqlite3_open_v2(SQLITE_RAS_DB, &db,
			     SQLITE_OPEN_FULLMUTEX | SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL);
	if (rc != SQLITE_OK) {
		printf("Failed to connect to %s: error = %d\n",
		       ENABLE_RAS_MC_EVENT, rc);
		return NULL;
	}

	strcpy(sql, createdb);
	strcat(sql, mc_event_db);
	strcat(sql, mc_event_db_create_fields);
printf("%s\n", sql);
	rc = sqlite3_exec(db, sql, NULL, NULL, NULL);
	if (rc != SQLITE_OK) {
		printf("Failed to create db on %s: error = %d\n",
		       ENABLE_RAS_MC_EVENT, rc);

		return NULL;
	}

	strcpy(sql, insertdb);
	strcat(sql, mc_event_db);
	strcat(sql, mc_event_db_fields);
	strcat(sql, valuesdb);

	strcat(sql, "(NULL, ");	/* Auto-increment field */
	for (i = 1; i < NUM_MC_EVENT_DB_VALUES; i++) {
		if (i < NUM_MC_EVENT_DB_VALUES - 1)
			strcat(sql, "?, ");
		else
			strcat(sql, "?)");
	}
printf("%s\n", sql);
	rc = sqlite3_prepare_v2(db, sql, -1, &ras->stmt, NULL);
	if (rc != SQLITE_OK) {
		printf("Failed to prepare insert db on %s: error = %s\n",
		       ENABLE_RAS_MC_EVENT, sqlite3_errmsg(db));
		return NULL;
	}

	return db;
}

static int ras_mc_event_handler(struct trace_seq *s,
				struct pevent_record *record,
				struct event_format *event, void *context)
{
	int len, rc;
	unsigned long long val;
	unsigned long long address, syndrome, grain;
	const char *msg, *label, *driver_detail, *error_type;
	int error_count;
	unsigned char mc_index;
	signed char top_layer, middle_layer, lower_layer;
	struct ras_events *ras = context;
	struct timeval tv;
	struct tm *tm;
	char fmt[64], timestamp[64] = "";

	tv.tv_sec = record->ts / 1000000L;
	tv.tv_usec = record->ts % 1000000L;

	tm = localtime(&tv.tv_sec);
	if(tm) {
		strftime(fmt, sizeof(fmt), "%Y-%m-%d %H:%M:%S.%%06u %z", tm);
		snprintf(timestamp, sizeof(timestamp), fmt, tv.tv_usec);
	}

	/* Doesn't work... we need a hack here */
trace_seq_printf(s, "%s(%lld = %ld.%ld) ", timestamp, record->ts, (long)tv.tv_sec, (long)tv.tv_usec);

	if (pevent_get_field_val(s,  event, "error_count", record, &val, 0) < 0)
		return -1;
	error_count = val;
	trace_seq_printf(s, "%d ", error_count);

	if (pevent_get_field_val(s, event, "error_type", record, &val, 1) < 0)
		return -1;
	error_type = mc_event_error_type(val);
	trace_seq_puts(s, error_type);
	if (error_count > 1)
		trace_seq_puts(s, " errors:");
	else
		trace_seq_puts(s, " error:");

	msg = pevent_get_field_raw(s, event, "msg", record, &len, 1);
	if (!msg)
		return -1;
	if (*msg) {
		trace_seq_puts(s, " ");
		trace_seq_puts(s, msg);
	}

	label = pevent_get_field_raw(s, event, "label", record, &len, 1);
	if (!label)
		return -1;
	if (*label) {
		trace_seq_puts(s, " on ");
		trace_seq_puts(s, label);
	}

	trace_seq_puts(s, " (");
	if (pevent_get_field_val(s,  event, "mc_index", record, &val, 0) < 0)
		return -1;
	mc_index = val;
	trace_seq_printf(s, "mc: %d", mc_index);

	if (pevent_get_field_val(s,  event, "top_layer", record, &val, 0) < 0)
		return -1;
	top_layer = (int) val;
	if (pevent_get_field_val(s,  event, "middle_layer", record, &val, 0) < 0)
		return -1;
	middle_layer = (int) val;
	if (pevent_get_field_val(s,  event, "lower_layer", record, &val, 0) < 0)
		return -1;
	lower_layer = (int) val;

	if (top_layer == 255)
		top_layer = -1;
	if (middle_layer == 255)
		middle_layer = -1;
	if (lower_layer == 255)
		lower_layer = -1;

	if (top_layer >= 0 || middle_layer >= 0 || lower_layer >= 0) {
		if (lower_layer >= 0)
			trace_seq_printf(s, " location: %d:%d:%d",
					top_layer, middle_layer, lower_layer);
		else if (middle_layer >= 0)
			trace_seq_printf(s, " location: %d:%d",
					 top_layer, middle_layer);
		else
			trace_seq_printf(s, " location: %d", top_layer);
	}

	if (pevent_get_field_val(s,  event, "address", record, &val, 0) < 0)
		return -1;
	address = val;
	if (address)
		trace_seq_printf(s, " address: 0x%08llx", address);

	if (pevent_get_field_val(s,  event, "grain_bits", record, &val, 0) < 0)
		return -1;
	grain = val;
	trace_seq_printf(s, " grain: %lld", grain);


	if (pevent_get_field_val(s,  event, "syndrome", record, &val, 0) < 0)
		return -1;
	syndrome = val;
	if (val)
		trace_seq_printf(s, " syndrome: 0x%08llx", syndrome);

	driver_detail = pevent_get_field_raw(s, event, "driver_detail", record,
					     &len, 1);
	if (!driver_detail)
		return -1;
	if (*driver_detail) {
		trace_seq_puts(s, " ");
		trace_seq_puts(s, driver_detail);
	}
	trace_seq_puts(s, ")");

	/* Insert data into the SGBD */
	sqlite3_bind_text(ras->stmt,  1, timestamp, -1, NULL);
	sqlite3_bind_int (ras->stmt,  2, error_count);
	sqlite3_bind_text(ras->stmt,  3, error_type, -1, NULL);
	sqlite3_bind_text(ras->stmt,  4, msg, -1, NULL);
	sqlite3_bind_text(ras->stmt,  5, label, -1, NULL);
	sqlite3_bind_int (ras->stmt,  6, mc_index);
	sqlite3_bind_int (ras->stmt,  7, top_layer);
	sqlite3_bind_int (ras->stmt,  8, middle_layer);
	sqlite3_bind_int (ras->stmt,  9, lower_layer);
	sqlite3_bind_int (ras->stmt, 10, address);
	sqlite3_bind_int (ras->stmt, 11, grain);
	sqlite3_bind_int (ras->stmt, 12, syndrome);
	sqlite3_bind_text(ras->stmt, 13, driver_detail, -1, NULL);
	rc = sqlite3_step(ras->stmt);
	if (rc != SQLITE_OK && rc != SQLITE_DONE)
		printf("Failed to do step on sqlite: error = %d\n", rc);
	rc = sqlite3_finalize(ras->stmt);
	if (rc != SQLITE_OK && rc != SQLITE_DONE)
		printf("Failed to do finalize insert on sqlite: error = %d\n",
		       rc);

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

static int handle_ras_events(void)
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
	ras->db = ras_mc_event_opendb(ras);

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

/*
 * Arguments(argp) handling logic and main
 */

#define TOOL_NAME "rasdaemon"
#define TOOL_DESCRIPTION "RAS daemon to log the RAS events."
#define ARGS_DOC "<options>"

const char *argp_program_version = TOOL_NAME " " VERSION;
const char *argp_program_bug_address = "Mauro Carvalho Chehab <mchehab@redhat.com>";

struct arguments {
	int handle_events, enable_ras;
};

static error_t parse_opt(int k, char *arg, struct argp_state *state)
{
	struct arguments *args = state->input;

	switch (k) {
	case 'r':
		args->handle_events++;
		break;
	case 'e':
		args->enable_ras++;
		break;
	case 'd':
		args->enable_ras--;
		break;
	default:
		return ARGP_ERR_UNKNOWN;
	}
	return 0;
}

int main(int argc, char *argv[])
{
	struct arguments args;
	int idx = -1;
	const struct argp_option options[] = {
		{"read",    'r', 0, 0, "read RAS events", 0},
		{"enable",  'e', 0, 0, "enable RAS events", 0},
		{"disable", 'd', 0, 0, "disable RAS events", 0},

		{ 0, 0, 0, 0, 0, 0 }
	};
	const struct argp argp = {
		.options = options,
		.parser = parse_opt,
		.doc = TOOL_DESCRIPTION,
		.args_doc = ARGS_DOC,

	};
	memset (&args, 0, sizeof(args));

	argp_parse(&argp, argc, argv, 0,  &idx, &args);

	if (idx < 0) {
		argp_help(&argp, stderr, ARGP_HELP_STD_HELP, TOOL_NAME);
		return -1;
	}

	if (args.enable_ras > 0)
		toggle_ras_mc_event(1);
	else if (args.enable_ras < 0)
		toggle_ras_mc_event(0);

	if (args.handle_events)
		handle_ras_events();

	return 0;
}
