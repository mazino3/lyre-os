#include <stddef.h>
#include <stdint.h>
#include <lib/event.h>
#include <lib/lock.h>
#include <sched/sched.h>
#include <sys/cpu.h>

static ssize_t check_for_pending(struct event **events, size_t count) {
    for (size_t i = 0; i < count; i++) {
        if (events[i]->pending > 0) {
            events[i]->pending--;
            return i;
        }
    }

    return -1;
}

static void attach_listeners(struct event **events, size_t count, struct thread *thread) {
    for (size_t i = 0; i < count; i++) {
        struct event *event = events[i];

        if (event->listeners_i == EVENT_MAX_LISTENERS) {
            print("PANIC event listeners exhausted\n");
            for (;;);
        }

        struct event_listener *listener = &event->listeners[event->listeners_i];

        listener->thread = thread;
        listener->which = i;

        event->listeners_i++;
    }
}

static void lock_events(struct event **events, size_t count) {
    for (size_t i = 0; i < count; i++) {
        SPINLOCK_ACQUIRE(events[i]->lock);
    }
}

static void unlock_events(struct event **events, size_t count) {
    for (size_t i = 0; i < count; i++) {
        LOCK_RELEASE(events[i]->lock);
    }
}

bool events_await(struct event **events, ssize_t *which, size_t count, bool no_block) {
    bool block = !no_block;

    struct thread *thread = this_cpu->current_thread;

    asm ("cli");

    lock_events(events, count);

    ssize_t i = check_for_pending(events, count);
    if (i >= 0) {
        unlock_events(events, count);
        *which = i;
        return true;
    }

    if (block == false) {
        unlock_events(events, count);
        return false;
    }

    attach_listeners(events, count, thread);

    sched_dequeue(thread);

    unlock_events(events, count);

    sched_yield();

    *which = thread->which_event;

    return true;
}

size_t event_trigger(struct event *event) {
    size_t ret;

    bool ints = interrupt_state();

    asm ("cli");

    SPINLOCK_ACQUIRE(event->lock);

    if (event->listeners_i == 0) {
        event->pending++;
        ret = 0;
        goto out;
    }

    for (size_t i = 0; i < event->listeners_i; i++) {
        struct thread *thread = event->listeners[i].thread;

        thread->which_event = event->listeners[i].which;

        sched_queue(thread);
    }

    ret = event->listeners_i;

    event->listeners_i = 0;

out:
    LOCK_RELEASE(event->lock);

    if (ints == true) {
        asm ("sti");
    }

    return ret;
}

struct event *event_create(void) {
    struct event *new = alloc(sizeof(struct event));
    return new;
}
