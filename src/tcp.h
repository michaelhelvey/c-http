/**
 * TCP connection utilties that starts a server.
 */

#pragma once

#include "common.h"

/**
 * Starts a TCP server on the given port.
 *
 * @param port The port to listen on.
 * @return The server socket file descriptor.
 */
i32 start_server(u16 port);

/**
 * Polls for a new connection on the given server socket.
 *
 * @param server_fd The server socket file descriptor.
 * @return An async result indicating whether the poll is ready or pending, and the client socket
 * file descriptor if the poll is ready.
 */
async_result_t poll_accept_connection(i32 server_fd);
