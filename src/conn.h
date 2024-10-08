/**
 * Utilies connecting futures to file descriptors.
 */

#pragma once

#include "common.h"
#include "handler.h"

typedef struct conn_map_entry_t {
    int fd;
    handler_future_t* future;
    struct conn_map_entry_t* next;
} conn_map_entry_t;

typedef struct conn_map_t {
    size_t cap;
    conn_map_entry_t** buckets;
} conn_map_t;

/**
 * Creates a new connection map with the given capacity.
 */
conn_map_t* conn_map_new(size_t capacity);

/**
 * Inserts a new connection into the map.
 */
void conn_map_insert(conn_map_t* self, int fd, handler_future_t* future);

/**
 * Retrieves the connection from the map.
 */
handler_future_t* conn_map_get(conn_map_t* self, int fd);

/**
 * Removes the connection from the map.
 */
void conn_map_remove(conn_map_t* self, int fd);

int conn_test_suite();
