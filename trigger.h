#ifndef __TRIGGER_H__
#define __TRIGGER_H__

struct event_trigger {
        const char *name;
        void (*setup)(void);
};

int trigger_check(char *s);
void run_trigger(const char *trigger, char *argv[], char **env, const char* reporter);


#endif
