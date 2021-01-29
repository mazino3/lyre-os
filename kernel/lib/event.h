#ifndef __LIB__EVENT_H__
#define __LIB__EVENT_H__

#include <stddef.h>
#include <stdbool.h>
#include <sched/sched.h>
#include <lib/lock.h>

struct event_listener {
    lock_t lock;
    lock_t ready;
    struct thread *thread;
    void *bitmap;
    size_t bitmap_offset;
};

struct event {
    size_t pending;
    size_t listener_count;
    struct event_listener listeners[];
};

struct event *event_create(size_t max_listeners);
bool events_await(struct event **events, void *bitmap, size_t event_count);
void event_trigger(struct event *event);

#endif
