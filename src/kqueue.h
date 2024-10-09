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
 * Register yourself for read events on the given file descriptor.
 */
int register_read_event(int fd);

/**
 * Register yourself for write events on the given file descriptor.
 */
int register_write_event(int fd);

/**
 * Blocks the thread until one or more registered events are triggered. Returns the number of events
 * that were triggered.
 */
int block_until_events();

typedef struct eventlist_iter_t {
    size_t index;
} eventlist_iter_t;

/**
 * Returns an iterator to the list of events that were triggered.
 */
eventlist_iter_t get_eventlist_iter();

/**
 * Returns the next event that was triggered. Returns NULL if there are no more events.
 */
struct kevent* get_next_event(eventlist_iter_t* iter);

int kqueue_test_suite();
