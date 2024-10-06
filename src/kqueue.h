#pragma once

#include "common.h"
#include <sys/event.h>
#include <sys/time.h>
#include <sys/types.h>

/**
 * Initialize the kqueue subsystem.
 */
void kqueue_init();

typedef enum {
    EVENT_READ,
    EVENT_WRITE,
} kqueue_event_t;

/**
 * Listen for events on a file descriptor. Flags specifies whether you want to register for read or
 * write events, or both.
 */
i32 register_events(u32 fd, usize flags);

/**
 * Blocks the thread until one or more registered events are triggered. Returns the number of events
 * that were triggered.
 */
i32 block_until_events();

typedef struct eventlist_iter_t {
    usize index;
} eventlist_iter_t;

/**
 * Returns an iterator to the list of events that were triggered.
 */
eventlist_iter_t get_eventlist_iter();

/**
 * Returns the next event that was triggered. Returns NULL if there are no more events.
 */
struct kevent* get_next_event(eventlist_iter_t* iter);

i32 kqueue_test_suite();
