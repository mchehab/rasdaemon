#ifndef __TRIGGER_H__
#define __TRIGGER_H__

struct event_trigger {
	const char *name;
	void (*setup)(void);
};

const char *trigger_check(const char *s);
void run_trigger(const char *trigger, char *argv[], char **env, const char *reporter);

#endif
