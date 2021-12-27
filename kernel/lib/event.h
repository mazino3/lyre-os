#ifndef __LIB__EVENT_H__
#define __LIB__EVENT_H__

#include <stddef.h>
#include <stdbool.h>
#include <sched/sched.h>
#include <lib/lock.h>
#include <lib/types.h>

#define EVENT_MAX_LISTENERS 32

struct event_listener {
    struct thread *thread;
    size_t which;
};

struct event {
    lock_t lock;
    size_t pending;
    size_t listeners_i;
    struct event_listener listeners[EVENT_MAX_LISTENERS];
};

struct event *event_create(void);
bool events_await(struct event **events, ssize_t *which, size_t event_count,
                  bool no_block);
size_t event_trigger(struct event *event);

#endif
