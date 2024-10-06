#include "kqueue.h"

static u32 queue_fd;

void kqueue_init()
{
    i32 r = kqueue();
    if (r == -1) {
        perror("kqueue");
        exit(1);
    }

    queue_fd = r;
}

i32 register_events(u32 fd, usize events)
{
    if (events == 0) {
        return 0;
    }

    struct kevent changelist[2];

    if (events & EVENT_READ) {
        EV_SET(&changelist[0], fd, EVFILT_READ, EV_ADD | EV_ONESHOT, 0, 0, NULL);
    }

    if (events & EVENT_WRITE) {
        EV_SET(&changelist[1], fd, EVFILT_WRITE, EV_ADD | EV_ONESHOT, 0, 0, NULL);
    }

    u32 event_count = (events & EVENT_READ && events & EVENT_WRITE) ? 2 : 1;

    if (kevent(queue_fd, changelist, event_count, NULL, 0, NULL) == -1) {
        perror("kevent");
        return -1;
    }

    return 0;
}

// Note that this effectively places an upper bound on the number of incoming client handlers we can
// have in a "ready" state at any one time
#define KQUEUE_MAX_EVENTS 1024
struct kevent eventlist[KQUEUE_MAX_EVENTS];
usize last_event_len = 0;

i32 block_until_events()
{
    i32 event_count = kevent(queue_fd, NULL, 0, eventlist, KQUEUE_MAX_EVENTS, NULL);
    if (event_count == -1) {
        perror("kevent");
        return -1;
    }

    last_event_len = event_count;

    return 0;
}

eventlist_iter_t get_eventlist_iter() { return (eventlist_iter_t) { 0 }; }

struct kevent* get_next_event(eventlist_iter_t* iter)
{
    if (iter->index >= last_event_len) {
        return NULL;
    }

    return &eventlist[iter->index++];
}

/**
 ***************************************************************************************************
 * Test suite
 ***************************************************************************************************
 */

i32 kqueue_test_suite()
{
    println("TODO: Implement kqueue test suite");
    return 0;
}
