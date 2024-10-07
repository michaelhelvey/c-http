#include "kqueue.h"
#include <pthread.h>
#include <stdio.h>
#include <sys/_pthread/_pthread_t.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

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

i32 register_read_event(u32 fd)
{
    struct kevent changelist[1];
    EV_SET(&changelist[0], fd, EVFILT_READ, EV_ADD | EV_ONESHOT, 0, 0, NULL);

    if (kevent(queue_fd, changelist, 1, NULL, 0, NULL) == -1) {
        perror("kevent");
        return -1;
    }

    return 0;
}

i32 register_write_event(u32 fd)
{
    struct kevent changelist[1];
    EV_SET(&changelist[0], fd, EVFILT_WRITE, EV_ADD | EV_ONESHOT, 0, 0, NULL);

    if (kevent(queue_fd, changelist, 1, NULL, 0, NULL) == -1) {
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
 * Tests
 ***************************************************************************************************
 */
#define LOG_IN_TEST_SUITE 0
#define test_log(fmt, ...)                                                                         \
    do {                                                                                           \
        if (LOG_IN_TEST_SUITE) {                                                                   \
            printf(fmt, ##__VA_ARGS__);                                                            \
        }                                                                                          \
    } while (0)

#define assert(cond, msg)                                                                          \
    do {                                                                                           \
        if (!(cond)) {                                                                             \
            printf("Assertion failed: %s\n", msg);                                                 \
            return -1;                                                                             \
        }                                                                                          \
    } while (0)

void* server_thread(void* message)
{
    char* msg = (char*)message;
    // create a socket that sends <message> to any client that connects
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    struct sockaddr_un addr;
    strcpy(addr.sun_path, "./test.sock");
    unlink(addr.sun_path);
    addr.sun_family = AF_UNIX;

    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        return (void*)-1;
    }

    if (listen(sock, 5) < 0) {
        perror("listen");
        return (void*)-1;
    }

    test_log("[server]: waiting for client connection\n");
    int client_sock = accept(sock, NULL, NULL);
    test_log("[server]: accepted client connection\n");
    if (client_sock < 0) {
        perror("accept");
        return (void*)-1;
    }

    if (write(client_sock, msg, strlen(msg)) < 0) {
        perror("write");
        return (void*)-1;
    }

    close(client_sock);
    return (void*)0;
}

i32 test_kqueue_register_read_event()
{
    kqueue_init();

    const char* message = "Hello, world!";

    pthread_t server_thread_id;
    if (pthread_create(&server_thread_id, NULL, server_thread, (void*)message) < 0) {
        perror("pthread_create");
        return -1;
    }

    // This is a bit hacky but we need to wait for the server thread to start and create the socket:
    nanosleep(&(struct timespec) { .tv_nsec = 1000000 }, NULL);

    // now that we have a server in another thread, we just need to create a client
    // that connects to the server and reads the message
    int client_sock = socket(AF_UNIX, SOCK_STREAM, 0);
    struct sockaddr_un addr;
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, "./test.sock");

    if (connect(client_sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect");
        return -1;
    }

    if (register_read_event(client_sock) < 0) {
        return -1;
    }

    test_log("[client]: blocking until read events available\n");
    block_until_events();

    eventlist_iter_t iter = get_eventlist_iter();
    struct kevent* event = get_next_event(&iter);
    // We should only have one event since we only registered one
    assert((int)event->ident == client_sock, "client socket not in event list");

    // read the server message into our buffer:
    char buffer[1024];
    ssize_t bytes_read = read(client_sock, buffer, sizeof(buffer));
    if (bytes_read < 0) {
        perror("read");
        return -1;
    }
    buffer[bytes_read] = '\0';
    test_log("[client]: read message: %s\n", buffer);
    assert(strcmp(buffer, message) == 0, "message read from server does not match");

    i32 retval;
    if (pthread_join(server_thread_id, (void**)&retval) < 0) {
        perror("pthread_join");
        return -1;
    }

    if (retval != 0) {
        printf("ERROR: server_thread returned with: %d\n", retval);
        return -1;
    }

    return 0;
}

i32 kqueue_test_suite()
{
    i32 r = 0;
    if (test_kqueue_register_read_event() < 0) {
        r = -1;
        printf("\t❌ test_kqueue_register_read_event\n");
    } else {
        printf("\t✅ test_kqueue_register_read_event\n");
    }
    return r;
}
