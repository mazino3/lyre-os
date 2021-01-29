#include <stddef.h>
#include <stdint.h>
#include <lib/event.h>
#include <lib/lock.h>
#include <lib/bitmap.h>
#include <sched/sched.h>

static struct event_listener *get_listener(struct event *event) {
    for (size_t i = 0; i < event->listener_count; i++) {
        if (LOCK_ACQUIRE(event->listeners[i].lock))
            return &event->listeners[i];
    }
    return NULL;
}

bool events_await(struct event **events, void *bitmap, size_t event_count) {
    struct thread *thread = this_cpu->current_thread;

    LOCK_RELEASE(thread->event_block_requeue);

    for (size_t i = 0; i < event_count; i++) {
        bitmap_unset(bitmap, i);
        struct event_listener *listener = get_listener(events[i]);
        if (listener == NULL)
            return false;
        listener->thread        = this_cpu->current_thread;
        listener->bitmap        = bitmap;
        listener->bitmap_offset = i;
        LOCK_ACQUIRE(listener->ready);

        if (LOCKED_READ(events[i]->pending) > 0) {
            LOCK_RELEASE(listener->lock);
            LOCK_RELEASE(listener->ready);
            LOCKED_DEC(events[i]->pending);
            bitmap_set(bitmap, i);
            return true;
        }
    }

    dequeue_and_yield(&thread->event_block_requeue);

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

        pending = false;

        bitmap_set(listener->bitmap, listener->bitmap_offset);

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
