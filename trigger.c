#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include "ras-logger.h"
#include "trigger.h"

void run_trigger(const char *trigger, char *argv[], char **env, const char *reporter)
{
	pid_t child;
	char *path;
	int status;
	char *trigger_dir = getenv("TRIGGER_DIR");

	log(SYSLOG, LOG_INFO, "Running trigger `%s' (reporter: %s)\n", trigger, reporter);

	if (asprintf(&path, "%s/%s", trigger_dir, trigger) < 0)
		return;

	child = fork();
	if (child < 0) {
		log(SYSLOG, LOG_ERR, "Cannot create process for trigger");
		return;
	}

	if (child == 0) {
		execve(path, argv, env);
		_exit(127);
	} else {
		waitpid(child, &status, 0);
		if (WIFEXITED(status) && WEXITSTATUS(status)) {
			log(SYSLOG, LOG_INFO, "Trigger %s exited with status %d",
				trigger, WEXITSTATUS(status));
		} else if (WIFSIGNALED(status)) {
			log(SYSLOG, LOG_INFO, "Trigger %s killed by signal %d",
				trigger, WTERMSIG(status));
		}
	}
}

int trigger_check(const char *s)
{
	char *name;
	int rc;
	char *trigger_dir = getenv("TRIGGER_DIR");

	if (trigger_dir) {
		if (asprintf(&name, "%s/%s", trigger_dir, s) < 0)
			return -1;
	} else
		name = s;

	rc = access(name, R_OK|X_OK);

	if (trigger_dir)
		free(name);

	return rc;
}
