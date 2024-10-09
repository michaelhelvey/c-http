#include "common.h"
#include "tcp.h"
#include <errno.h>
#include <netinet/in.h>
#include <sys/fcntl.h>
#include <sys/socket.h>

int start_server(int port)
{

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        exit(1);
    }

    struct sockaddr_in server_addr
        = { .sin_family = AF_INET, .sin_port = htons(port), .sin_addr.s_addr = INADDR_ANY };

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        exit(1);
    }

    int flags = fcntl(server_fd, F_GETFL, 0);
    fcntl(server_fd, F_SETFL, flags | O_NONBLOCK);

    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        exit(1);
    }

    // I'm setting the backlog to 0 here so that things will fail quickly if we screw up our async
    // implementation and block too much (e.g. on file I/O).
    if (listen(server_fd, 0) < 0) {
        perror("listen");
        exit(1);
    }

    return server_fd;
}

async_result_t poll_accept_connection(int server_fd)
{
    int client_fd = accept(server_fd, NULL, NULL);

    if (errno == EWOULDBLOCK) {
        return (async_result_t) { .result = POLL_PENDING, .value = NULL };
    } else if (client_fd < 0) {
        perror("accept");
        return (async_result_t) { .result = POLL_READY, .value = (void*)-1 };
    }

    println("accepted connection from client_fd: %d", client_fd);

    int flags = fcntl(client_fd, F_GETFL, 0);
    if (fcntl(client_fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        perror("fcntl");
        return (async_result_t) { .result = POLL_READY, .value = (void*)-1 };
    }

    size_t return_value = client_fd;
    return (async_result_t) { .result = POLL_READY, .value = (void*)return_value };
}
