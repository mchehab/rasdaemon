// SPDX-License-Identifier: GPL-2.0

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "ras-logger.h"
#include "types.h"
#include "trigger.h"

#include "ras-mce-handler.h"

#define MAX_ENV 30
static int child_done, alarm_done;
static char *trigger_dir;

static void child_handler(int sig)
{
	child_done = 1;
}

static void alarm_handler(int sig)
{
	alarm_done = 1;
}

void run_trigger(struct event_trigger *t, char *argv[], char **env)
{
	pid_t child;
	char *trigger = t->path;
	const char *path = t->abs_path;
	int status, timeout = t->timeout;

	log(ALL, LOG_INFO, "Running trigger `%s' (reporter: %s)\n",
	    trigger, t->event_name);

	child = fork();
	if (child < 0) {
		log(ALL, LOG_ERR, "Cannot create process for trigger\n");
		return;
	} else if (child == 0) {
		if (execve(path, argv, env) == -1)
			log(ALL, LOG_ERR, "Trigger %s exec fail: %s\n", path, strerror(errno));
		_exit(EXIT_FAILURE);
	}

	signal(SIGCHLD, child_handler);
	if (timeout) {
		signal(SIGALRM, alarm_handler);
		alarm(timeout);
	}
	pause();

	if (child_done) {
		if (waitpid(child, &status, WNOHANG) == child) {
			if (WIFEXITED(status) && WEXITSTATUS(status))
				log(ALL, LOG_INFO,
				    "Trigger %s exited with status %d\n",
				    trigger, WEXITSTATUS(status));
			else if (WIFSIGNALED(status))
				log(ALL, LOG_INFO,
				    "Trigger %s killed by signal %d\n",
				    trigger, WTERMSIG(status));
		}
		alarm(0);
	} else if (alarm_done) {
		log(ALL, LOG_WARNING, "Trigger timeout, kill it\n");
		kill(child, SIGKILL);
	}

	signal(SIGCHLD, SIG_DFL);
	signal(SIGALRM, SIG_DFL);
}

int trigger_check(struct event_trigger *t)
{
	if (trigger_dir)
		if (snprintf(t->abs_path, 256, "%s/%s", trigger_dir, t->path) < 0)
			return -1;

	return access(t->abs_path, R_OK | X_OK);
}

struct event_trigger mc_ue_trigger = {"mc_event", "MC_UE_TRIGGER"};

struct event_trigger mce_de_trigger = {"mce_record", "MCE_DE_TRIGGER"};
struct event_trigger mce_ue_trigger = {"mce_record", "MCE_UE_TRIGGER"};

struct event_trigger mf_trigger = {"memory_failure_event", "MEM_FAIL_TRIGGER"};

struct event_trigger aer_ce_trigger = {"aer_event", "AER_CE_TRIGGER"};
struct event_trigger aer_ue_trigger = {"aer_event", "AER_UE_TRIGGER"};
struct event_trigger aer_fatal_trigger = {"aer_event", "AER_FATAL_TRIGGER"};

static struct event_trigger *event_triggers[] = {
	&mc_ue_trigger,
#ifdef HAVE_MCE
	&mce_de_trigger,
	&mce_ue_trigger,
#endif
#ifdef HAVE_MEMORY_FAILURE
	&mf_trigger,
#endif
#ifdef HAVE_AER
	&aer_ce_trigger,
	&aer_ue_trigger,
	&aer_fatal_trigger,
#endif
};

void setup_event_trigger(const char *event)
{
	int i, j;
	struct event_trigger *trigger;
	char *s, timeout_env[30];

	trigger_dir = getenv("TRIGGER_DIR");

	for (i = 0; i < ARRAY_SIZE(event_triggers); i++) {
		trigger = event_triggers[i];

		if (strcmp(event, trigger->event_name))
			continue;

		s = getenv(trigger->env);
		if (!s || !strcmp(s, ""))
			continue;

		trigger->path = s;
		if (trigger_check(trigger)) {
			log(ALL, LOG_ERR, "Cannot access trigger `%s`: %s\n", s, strerror(errno));
			continue;
		}

		log(ALL, LOG_NOTICE, "Setup %s trigger `%s`\n", trigger->event_name, s);

		snprintf(timeout_env, sizeof(timeout_env), "%s_TIMEOUT", trigger->env);

		trigger->timeout = 1;
		s = getenv(timeout_env);
		if (!s || !strcmp(s, "")) {
			log(ALL, LOG_NOTICE,
			    "Setup %s trigger default timeout 1s\n",
			    trigger->event_name);
			continue;
		}

		j = atoi(s);
		if (j < 0)
			log(ALL, LOG_ERR,
			    "Invalid %s trigger timeout `%d` use default value: 1s\n",
			    trigger->event_name, j);
		else if (j == 0) {
			log(ALL, LOG_NOTICE,
			    "%s trigger no timeout\n",
			    trigger->event_name);
			trigger->timeout = 0;
		} else {
			log(ALL, LOG_NOTICE,
			    "Setup %s trigger timeout `%d`s\n",
			    trigger->event_name, j);
			trigger->timeout = j;
		}
	}
}

static void __run_mce_trigger(struct mce_event *e, struct event_trigger *trigger)
{
	char *env[MAX_ENV];
	int ei = 0, i;

	if (!trigger->path || !strcmp(trigger->path, ""))
		return;

	if (asprintf(&env[ei++], "PATH=%s", getenv("PATH") ?: "/sbin:/usr/sbin:/bin:/usr/bin") < 0)
		goto free;
	if (asprintf(&env[ei++], "MCGCAP=%#lx", e->mcgcap) < 0)
		goto free;
	if (asprintf(&env[ei++], "MCGSTATUS=%#lx", e->mcgstatus) < 0)
		goto free;
	if (asprintf(&env[ei++], "STATUS=%#lx", e->status) < 0)
		goto free;
	if (asprintf(&env[ei++], "ADDR=%#lx", e->addr) < 0)
		goto free;
	if (asprintf(&env[ei++], "MISC=%#lx", e->misc) < 0)
		goto free;
	if (asprintf(&env[ei++], "IP=%#lx", e->ip) < 0)
		goto free;
	if (asprintf(&env[ei++], "TSC=%#lx", e->tsc) < 0)
		goto free;
	if (asprintf(&env[ei++], "WALLTIME=%#lx", e->walltime) < 0)
		goto free;
	if (asprintf(&env[ei++], "CPU=%#x", e->cpu) < 0)
		goto free;
	if (asprintf(&env[ei++], "CPUID=%#x", e->cpuid) < 0)
		goto free;
	if (asprintf(&env[ei++], "APICID=%#x", e->apicid) < 0)
		goto free;
	if (asprintf(&env[ei++], "SOCKETID=%#x", e->socketid) < 0)
		goto free;
	if (asprintf(&env[ei++], "CS=%#x", e->cs) < 0)
		goto free;
	if (asprintf(&env[ei++], "BANK=%#x", e->bank) < 0)
		goto free;
	if (asprintf(&env[ei++], "CPUVENDOR=%#x", e->cpuvendor) < 0)
		goto free;
	if (asprintf(&env[ei++], "SYND=%#lx", e->synd) < 0)
		goto free;
	if (asprintf(&env[ei++], "IPID=%#lx", e->ipid) < 0)
		goto free;
	if (asprintf(&env[ei++], "TIMESTAMP=%s", e->timestamp) < 0)
		goto free;
	if (asprintf(&env[ei++], "BANK_NAME=%s", e->bank_name) < 0)
		goto free;
	if (asprintf(&env[ei++], "ERROR_MSG=%s", e->error_msg) < 0)
		goto free;
	if (asprintf(&env[ei++], "MCGSTATUS_MSG=%s", e->mcgstatus_msg) < 0)
		goto free;
	if (asprintf(&env[ei++], "MCISTATUS_MSG=%s", e->mcistatus_msg) < 0)
		goto free;
	if (asprintf(&env[ei++], "MCASTATUS_MSG=%s", e->mcastatus_msg) < 0)
		goto free;
	if (asprintf(&env[ei++], "USER_ACTION=%s", e->user_action) < 0)
		goto free;
	if (asprintf(&env[ei++], "MC_LOCATION=%s", e->mc_location) < 0)
		goto free;
	env[ei] = NULL;
	assert(ei < MAX_ENV);

	run_trigger(trigger, NULL, env);

free:
	for (i = 0; i < ei; i++)
		free(env[i]);
}

void run_mce_record_trigger(struct mce_event *e)
{
	if (e->status & MCI_STATUS_UC)
		__run_mce_trigger(e, &mce_ue_trigger);
	else if (e->status & MCI_STATUS_DEFERRED)
		__run_mce_trigger(e, &mce_de_trigger);
}

static void __run_mc_trigger(struct ras_mc_event *ev, struct event_trigger *trigger)
{
	char *env[MAX_ENV];
	int ei = 0, i;

	if (!trigger->path || !strcmp(trigger->path, ""))
		return;

	if (asprintf(&env[ei++], "PATH=%s", getenv("PATH") ?: "/sbin:/usr/sbin:/bin:/usr/bin") < 0)
		goto free;
	if (asprintf(&env[ei++], "TIMESTAMP=%s", ev->timestamp) < 0)
		goto free;
	if (asprintf(&env[ei++], "COUNT=%d", ev->error_count) < 0)
		goto free;
	if (asprintf(&env[ei++], "TYPE=%s", ev->error_type) < 0)
		goto free;
	if (asprintf(&env[ei++], "MESSAGE=%s", ev->msg) < 0)
		goto free;
	if (asprintf(&env[ei++], "LABEL=%s", ev->label) < 0)
		goto free;
	if (asprintf(&env[ei++], "MC_INDEX=%d", ev->mc_index) < 0)
		goto free;
	if (asprintf(&env[ei++], "TOP_LAYER=%d", ev->top_layer) < 0)
		goto free;
	if (asprintf(&env[ei++], "MIDDLE_LAYER=%d", ev->middle_layer) < 0)
		goto free;
	if (asprintf(&env[ei++], "LOWER_LAYER=%d", ev->lower_layer) < 0)
		goto free;
	if (asprintf(&env[ei++], "ADDRESS=%llx", ev->address) < 0)
		goto free;
	if (asprintf(&env[ei++], "GRAIN=%lld", ev->grain) < 0)
		goto free;
	if (asprintf(&env[ei++], "SYNDROME=%llx", ev->syndrome) < 0)
		goto free;
	if (asprintf(&env[ei++], "DRIVER_DETAIL=%s", ev->driver_detail) < 0)
		goto free;
	env[ei] = NULL;
	assert(ei < MAX_ENV);

	run_trigger(trigger, NULL, env);

free:
	for (i = 0; i < ei; i++)
		free(env[i]);
}

void run_mc_event_trigger(struct ras_mc_event *e)
{
	if (!strcmp(e->error_type, "Uncorrected"))
		__run_mc_trigger(e, &mc_ue_trigger);
}

static void __run_mf_trigger(struct ras_mf_event *ev, struct event_trigger *trigger)
{
	char *env[MAX_ENV];
	int ei = 0;
	int i;

	if (!trigger->path || !strcmp(trigger->path, ""))
		return;

	if (asprintf(&env[ei++], "PATH=%s", getenv("PATH") ?: "/sbin:/usr/sbin:/bin:/usr/bin") < 0)
		goto free;
	if (asprintf(&env[ei++], "TIMESTAMP=%s", ev->timestamp) < 0)
		goto free;
	if (asprintf(&env[ei++], "PFN=%s", ev->pfn) < 0)
		goto free;
	if (asprintf(&env[ei++], "PAGE_TYPE=%s", ev->page_type) < 0)
		goto free;
	if (asprintf(&env[ei++], "ACTION_RESULT=%s", ev->action_result) < 0)
		goto free;

	env[ei] = NULL;
	assert(ei < MAX_ENV);

	run_trigger(trigger, NULL, env);

free:
	for (i = 0; i < ei; i++)
		free(env[i]);
}

void run_mf_event_trigger(struct ras_mf_event *e)
{
	__run_mf_trigger(e, &mf_trigger);
}

static void __run_aer_trigger(struct ras_aer_event *ev, struct event_trigger *trigger)
{
	char *env[MAX_ENV];
	int ei = 0;
	int i;

	if (!trigger->path || !strcmp(trigger->path, ""))
		return;

	if (asprintf(&env[ei++], "PATH=%s", getenv("PATH") ?: "/sbin:/usr/sbin:/bin:/usr/bin") < 0)
		goto free;
	if (asprintf(&env[ei++], "TIMESTAMP=%s", ev->timestamp) < 0)
		goto free;
	if (asprintf(&env[ei++], "ERROR_TYPE=%s", ev->error_type) < 0)
		goto free;
	if (asprintf(&env[ei++], "DEV_NAME=%s", ev->dev_name) < 0)
		goto free;
	if (asprintf(&env[ei++], "TLP_HEADER_VALID=%d", ev->tlp_header_valid) < 0)
		goto free;
	if (ev->tlp_header_valid)
		if (asprintf(&env[ei++], "TLP_HEADER=%08x %08x %08x %08x",
			     ev->tlp_header[0], ev->tlp_header[1],
			     ev->tlp_header[2], ev->tlp_header[3]) < 0)
			goto free;
	if (asprintf(&env[ei++], "MSG=%s", ev->msg) < 0)
		goto free;

	env[ei] = NULL;
	assert(ei < MAX_ENV);

	run_trigger(trigger, NULL, env);

free:
	for (i = 0; i < ei; i++)
		free(env[i]);
}

void run_aer_event_trigger(struct ras_aer_event *e)
{
	if (!strcmp(e->error_type, "Corrected"))
		__run_aer_trigger(e, &aer_ce_trigger);
	else if (!strcmp(e->error_type, "Uncorrected (Non-Fatal)"))
		__run_aer_trigger(e, &aer_ue_trigger);
	else if (!strcmp(e->error_type, "Uncorrected (Fatal)"))
		__run_aer_trigger(e, &aer_fatal_trigger);
}
