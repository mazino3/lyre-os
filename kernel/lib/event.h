#ifndef __LIB__EVENT_H__
#define __LIB__EVENT_H__

#include <stddef.h>
#include <stdbool.h>
#include <sched/sched.h>
#include <lib/lock.h>

struct event_listener {
    lock_t lock;
    struct thread *thread;
    int *ret;
};

struct event {
    size_t listener_count;
    struct event_listener listeners[];
};

struct event *event_create(size_t max_listeners);
bool events_await(struct event **events, int *returns, size_t event_count);
void event_trigger(struct event *event, int value);

#endif
