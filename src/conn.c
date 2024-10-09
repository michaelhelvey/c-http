#include "conn.h"
#include "handler.h"

conn_map_t* conn_map_new(size_t capacity)
{
    conn_map_t* self = malloc(sizeof(conn_map_t));
    self->cap = capacity;
    self->buckets = malloc(sizeof(conn_map_entry_t*) * capacity);
    for (size_t i = 0; i < capacity; i++) {
        self->buckets[i] = NULL;
    }
    return self;
}

void conn_map_insert(conn_map_t* self, int fd, handler_future_t* future)
{
    size_t index = fd % self->cap;
    conn_map_entry_t* prev = NULL;
    conn_map_entry_t* existing = self->buckets[index];

    // If there is already an entry in the bucket, update the future
    while (existing != NULL) {
        if (existing->fd == fd) {
            free_handler_future(existing->future);
            existing->future = future;
            return;
        }
        prev = existing;
        existing = existing->next;
    }

    // at this point, either prev will point to the last entry in the bucket that we should attach
    // to, or it will be null because the bucket was empty
    if (prev == NULL) {
        self->buckets[index] = malloc(sizeof(conn_map_entry_t));
        self->buckets[index]->fd = fd;
        self->buckets[index]->future = future;
        self->buckets[index]->next = NULL;
    } else {
        prev->next = malloc(sizeof(conn_map_entry_t));
        prev->next->fd = fd;
        prev->next->future = future;
        prev->next->next = NULL;
    }
}

handler_future_t* conn_map_get(conn_map_t* self, int fd)
{
    size_t index = fd % self->cap;
    conn_map_entry_t* existing = self->buckets[index];
    while (existing != NULL) {
        if (existing->fd == fd) {
            return existing->future;
        }
        existing = existing->next;
    }
    return NULL;
}

void conn_map_remove(conn_map_t* self, int fd)
{
    size_t index = fd % self->cap;
    conn_map_entry_t* prev = NULL;
    conn_map_entry_t* existing = self->buckets[index];
    while (existing != NULL) {
        if (existing->fd == fd) {
            // we found the entry, now we need to remove it
            if (prev == NULL) {
                self->buckets[index] = existing->next;
            } else {
                prev->next = existing->next;
            }
            free_handler_future(existing->future);
            free(existing);
            return;
        }
        prev = existing;
        existing = existing->next;
    }
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

int test_conn_map_insert_get_remove()
{
    conn_map_t* map = conn_map_new(10);
    handler_future_t* future = new_handler_future(24);

    conn_map_insert(map, 1, future);
    assert(conn_map_get(map, 1) == future, "conn_map_get should return the future we inserted");

    conn_map_remove(map, 1);
    assert(
        conn_map_get(map, 1) == NULL, "conn_map_get should return NULL after removing the future");

    // It's a test, I'm just leaking the map for now
    return 0;
}

int conn_test_suite()
{
    int r = 0;
    if (test_conn_map_insert_get_remove() < 0) {
        r = -1;
        printf("\t❌ test_conn_map_insert_get_remove\n");
    } else {
        printf("\t✅ test_conn_map_insert_get_remove\n");
    }
    return r;
}
