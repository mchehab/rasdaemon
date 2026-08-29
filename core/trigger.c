// SPDX-License-Identifier: GPL-2.0-only

#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#include "core/ras-logger.h"
#include "core/trigger.h"

/**
 * run_trigger - synchronously execute an external event reporter
 * @trigger: executable path
 * @argv: null-terminated argument vector
 * @env: null-terminated environment vector
 * @reporter: reporter name used in diagnostics
 *
 * The parent waits for the child. Fork and child-status failures are logged;
 * they are not returned to the caller.
 */
void run_trigger(const char *trigger, char *argv[], char **env, const char *reporter)
{
	pid_t child;
	int status;

	log(SYSLOG, LOG_INFO, "Running trigger `%s' (reporter: %s)\n", trigger, reporter);

	child = fork();
	if (child < 0) {
		log(SYSLOG, LOG_ERR, "Cannot create process for trigger");
		return;
	}

	if (child == 0) {
		execve(trigger, argv, env);
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

/**
 * trigger_check - resolve and validate a trigger executable
 * @s: trigger name or path
 *
 * When ``TRIGGER_DIR`` is set, the returned path is newly allocated and lives
 * for the remainder of the process. Otherwise the return value aliases @s.
 *
 * Return:
 * an executable, readable path, or NULL if validation fails.
 */
const char *trigger_check(const char *s)
{
	char *name;
	int rc;
	char *trigger_dir = getenv("TRIGGER_DIR");

	if (trigger_dir) {
		if (asprintf(&name, "%s/%s", trigger_dir, s) < 0)
			return NULL;
		s = name;
	}

	rc = access(s, R_OK | X_OK);

	if (!rc)
		return(s);

	return NULL;
}
