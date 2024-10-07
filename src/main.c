#include "common.h"
#include "conn.h"
#include "handler.h"
#include "kqueue.h"
#include "tcp.h"
#include <getopt.h>
#include <sys/event.h>
#include <unistd.h>

struct option long_options[] = { { "help", no_argument, 0, 'h' },
    { "version", no_argument, 0, 'v' }, { "port", required_argument, 0, 'p' } };

u16 port = 8080;
bool shutdown = false;
#define CONN_MAP_SIZE 1024

void on_signal(int sig)
{
    println("received signal %d, shutting down...", sig);
    shutdown = true;
}

int main(int argc, char* argv[])
{
    if (signal(SIGINT, on_signal) == SIG_ERR) {
        perror("failed to register signal handler");
        return 1;
    }

    switch (getopt_long(argc, argv, "hvp:", long_options, NULL)) {
    case 'h':
        printf("Usage: %s [OPTION]...\n", argv[0]);
        printf("Async http server for fun.\n\n");
        printf("  -p, --port=PORT specify the port to listen on\n");
        printf("  -h, --help      display this help and exit\n");
        printf("  -v, --version   output version information and exit\n");
        return 0;
    case 'v':
        printf("c-http %s\n", VERSION);
        return 0;
    case 'p':
        port = atoi(optarg);
        break;
    case -1:
        break;
    default:
        return 1;
    }

    println("starting server on port %d...", port);

    // Initialize subsystems:
    kqueue_init();
    i32 server_fd = start_server(port);
    register_read_event(server_fd);
    conn_map_t* conn_map = conn_map_new(CONN_MAP_SIZE);

    // Main event loop:
    while (!shutdown) {
        println("starting new event loop");
        block_until_events();

        eventlist_iter_t iter = get_eventlist_iter();
        struct kevent* event;
        while ((event = get_next_event(&iter)) != NULL) {
            if ((i32)event->ident == server_fd) {
                // we are ready to accept a new connection:
                async_result_t result = poll_accept_connection(server_fd);
                // first of all, we want to re-register the server_fd for read events, since no
                // matter what happens we want to be able to accept new connections
                register_read_event(server_fd);

                // There are 3 possible outcomes:
                // 1) kqueue lied to us and we're not actually ready to accept a connection.
                if (result.result == POLL_PENDING) {
                    // nothing to do, since we've already re-registered the server_fd
                    continue;
                }

                i32 return_val = (i32)(size_t)result.value;

                // 2) we successfully accepted a connection.
                if (result.result == POLL_READY && return_val > 0) {
                    // return_val is a file descriptor that we should register as an HTTP handler
                    handler_future_t* future = new_handler_future(return_val);
                    conn_map_insert(conn_map, return_val, future);
                    register_read_event(return_val);
                }

                // 3) we failed to accept the connection
                if (result.result == POLL_READY && return_val < 0) {
                    println("failed to accept connection, dropping it and moving on.");
                }

            } else {
                // we need to poll the handler associated with the event:
                handler_future_t* future = conn_map_get(conn_map, event->ident);
                if (future == NULL) {
                    println("no handler found for fd %ld, ignoring event", event->ident);
                    continue;
                }

                async_result_t result = poll_handler_future(future);
                // Once again, there are 3 possible outcomes:
                // 1) kqueue lied to us and we're not actually ready to read from the connection.
                if (result.result == POLL_PENDING) {
                    // nothing to do, since handler futures are responsible for re-registring
                    // themselves
                    continue;
                }

                i32 return_val = (i32)(size_t)result.value;
                if (result.result == POLL_READY && return_val < 0) {
                    // 2) our handler failed in some way, we just need to clean up and moveon
                    println("failure while handling connection %ld, dropping it.", event->ident);
                    conn_map_remove(conn_map, event->ident);
                    close(event->ident);
                }

                // 3) else, it succeeded, but we don't actually care about the return value --
                // handlers are responsible for cleaning themselves up, or keeping the connection
                // open, or whatever is appropriate based on the way they implement the HTTP
                // protocol.
            }
        }
    }

    println("event loop closed, exiting server program");
}
