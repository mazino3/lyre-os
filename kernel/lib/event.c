#include <stddef.h>
#include <stdint.h>
#include <lib/event.h>
#include <lib/lock.h>
#include <sched/sched.h>

static struct event_listener *get_listener(struct event *event) {
    for (size_t i = 0; i < event->listener_count; i++) {
        if (LOCK_ACQUIRE(event->listeners[i].lock))
            return &event->listeners[i];
    }
    return NULL;
}

bool events_await(struct event **events, int *returns, size_t event_count) {
    LOCK_RELEASE(this_cpu->current_thread->event_block_requeue);

    for (size_t i = 0; i < event_count; i++) {
        returns[i] = 0;
        struct event_listener *listener = get_listener(events[i]);
        if (listener == NULL)
            return false;
        LOCKED_WRITE(listener->ret, &returns[i]);
        LOCKED_WRITE(listener->thread, this_cpu->current_thread);
    }

    dequeue_and_yield(&this_cpu->current_thread->event_block_requeue);

    return true;
}

void event_trigger(struct event *event, int value) {
    for (size_t i = 0; i < event->listener_count; i++) {
        if (LOCK_ACQUIRE(event->listeners[i].lock)) {
            LOCK_RELEASE(event->listeners[i].lock);
            continue;
        }

        while (LOCKED_READ(event->listeners[i].ret) == NULL)
            ;
        *event->listeners[i].ret = value;
        event->listeners[i].ret = NULL;

        while (LOCKED_READ(event->listeners[i].thread) == NULL)
            ;
        struct thread *thread = event->listeners[i].thread;
        event->listeners[i].thread = NULL;

        LOCK_ACQUIRE(thread->event_block_requeue);
        sched_queue_back(thread);

        LOCK_RELEASE(event->listeners[i].lock);
    }
}

struct event *event_create(size_t max_listeners) {
    struct event *new = alloc(sizeof(struct event)
                            + sizeof(struct event_listener) * max_listeners);
    new->listener_count = max_listeners;
    return new;
}
