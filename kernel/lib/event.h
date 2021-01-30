#ifndef __LIB__EVENT_H__
#define __LIB__EVENT_H__

#include <stddef.h>
#include <stdbool.h>
#include <sched/sched.h>
#include <lib/lock.h>
#include <lib/types.h>

struct event_listener {
    lock_t lock;
    lock_t ready;
    struct thread *thread;
    size_t   index;
    ssize_t *which;
};

struct event {
    size_t pending;
    size_t listener_count;
    struct event_listener listeners[];
};

struct event *event_create(size_t max_listeners);
bool events_await(struct event **events, ssize_t *which, size_t event_count,
                  bool no_block);
void event_trigger(struct event *event);

#endif
