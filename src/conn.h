/**
 * Utilies connecting futures to file descriptors.
 */

#pragma once

#include "common.h"
#include "handler.h"

typedef struct conn_map_entry_t {
    i32 fd;
    handler_future_t* future;
    struct conn_map_entry_t* next;
} conn_map_entry_t;

typedef struct conn_map_t {
    usize cap;
    conn_map_entry_t** buckets;
} conn_map_t;

/**
 * Creates a new connection map with the given capacity.
 */
conn_map_t* conn_map_new(usize capacity);

/**
 * Inserts a new connection into the map.
 */
void conn_map_insert(conn_map_t* self, i32 fd, handler_future_t* future);

/**
 * Retrieves the connection from the map.
 */
handler_future_t* conn_map_get(conn_map_t* self, i32 fd);

/**
 * Removes the connection from the map.
 */
void conn_map_remove(conn_map_t* self, i32 fd);

i32 conn_test_suite();
