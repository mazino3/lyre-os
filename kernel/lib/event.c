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

bool events_await(struct event **events, ssize_t *which, size_t event_count,
                  bool no_block) {
    struct thread *thread = this_cpu->current_thread;

    LOCK_RELEASE(thread->event_block_requeue);
    LOCK_RELEASE(thread->event_occurred);

    struct event_listener *listeners[event_count];
    size_t listeners_armed = 0;

    for (size_t i = 0; i < event_count; i++) {
        if (LOCKED_READ(events[i]->pending) > 0
         && LOCK_ACQUIRE(thread->event_occurred)) {
            LOCKED_DEC(events[i]->pending);
            *which = i;
            goto unarm_listeners;
        }

        if (!LOCK_ACQUIRE(thread->event_occurred)) {
            goto unarm_listeners;
        }
        LOCK_RELEASE(thread->event_occurred);

        struct event_listener *listener = get_listener(events[i]);
        if (listener == NULL)
            return false;
        listener->thread = this_cpu->current_thread;
        listener->which  = which;
        listener->index  = i;
        LOCK_ACQUIRE(listener->ready);

        listeners[i] = listener;
        listeners_armed = i++;
    }

    if (no_block && LOCK_ACQUIRE(thread->event_occurred)) {
        *which = -1;
        goto unarm_listeners;
    }

    dequeue_and_yield(&thread->event_block_requeue);

unarm_listeners:
    for (size_t i = 0; i < listeners_armed; i++) {
        LOCK_RELEASE(listeners[i]->ready);
        LOCK_RELEASE(listeners[i]->lock);
    }

    return true;
}

void event_trigger(struct event *event) {
    if (LOCKED_READ(event->pending) > 0) {
        LOCKED_INC(event->pending);
        return;
    }

    bool pending = true;

    for (size_t i = 0; i < event->listener_count; i++) {
        struct event_listener *listener = &event->listeners[i];

        if (LOCK_ACQUIRE(listener->lock)) {
            LOCK_RELEASE(listener->lock);
            continue;
        }

        if (LOCK_ACQUIRE(listener->ready)) {
            LOCK_RELEASE(listener->ready);
            continue;
        }

        if (!LOCK_ACQUIRE(listener->thread->event_occurred)) {
            continue;
        }

        pending = false;

        *listener->which = listener->index;

        struct thread *thread = listener->thread;

        LOCK_ACQUIRE(thread->event_block_requeue);
        sched_queue_back(thread);

        LOCK_RELEASE(listener->lock);
        LOCK_RELEASE(listener->ready);
    }

    if (pending) {
        LOCKED_INC(event->pending);
    }
}

struct event *event_create(size_t max_listeners) {
    struct event *new = alloc(sizeof(struct event)
                            + sizeof(struct event_listener) * max_listeners);
    new->listener_count = max_listeners;
    return new;
}
