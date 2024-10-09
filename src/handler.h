#pragma once

#include "arena.h"
#include "common.h"

// A string view represents a pointer into the read buffer owned by the HTTP request handler. During
// parsing, we simply identify the starting index and length of the string we're interested in, and
// then we can use this view to access the string without copying it.
typedef struct string_view_t {
    const char* data;
    size_t len;
} string_view_t;

bool string_view_equals(string_view_t a, string_view_t b);

typedef struct header_value_t {
    string_view_t* value;
    struct header_value_t* next;
} header_value_t;

typedef struct header_t {
    string_view_t key;
    header_value_t value;
    // FIXME: write a proper hash map for this with string hashing etc.
    struct header_t* next;
} header_t;

typedef struct request_t {
    string_view_t method;
    string_view_t path;
    string_view_t version;
    header_t* headers;
} request_t;

typedef enum handler_future_state_t {
    HANDLER_READING,
    HANDLER_WRITING,
    HANDLER_DONE,
} handler_future_state_t;

typedef struct read_stream_t {
    // Buffer holding stream data read from the client file descriptor
    char* data;
    // The total allocated length of the buffer
    size_t len;
    // How far into the allocated length of the buffer we should write new data on the next `read`
    // call.
    size_t write_cursor;
    // How far the user has actually read from the stream via calls like `read_line`.
    size_t read_cursor;
} read_stream_t;

typedef struct write_stream_t {
    // The buffer of data that we are trying to write. This memory is expected to be owned by the
    // caller.
    char* data;
    // The total length of the buffer
    size_t len;
    // How far we have written so far, so we know where to resume on the next write call.
    size_t cursor;
} write_stream_t;

typedef struct handler_future_t {
    // The client file descriptor we're handling
    int fd;
    // The memory arena that we use to allocate memory for the request we are parsing
    arena_t* arena;
    // The request we are currently parsing
    request_t request;
    // The state of the future, determining which "point" in the handler we should resume when
    // we are polled again.
    handler_future_state_t state;
    // Buffer state that we read into from the client fd
    read_stream_t read_stream;
    // Buffer state that we write into to the client fd
    write_stream_t* write_stream;
} handler_future_t;

// Values that can be returned as the `value` parameter of the handler's async result.
typedef enum handler_return_t {
    HANDLER_KEEPALIVE,
    HANDLER_CLOSE,
} handler_return_t;

handler_future_t* new_handler_future(int fd);

async_result_t poll_handler_future(handler_future_t* self);

void free_handler_future(handler_future_t* self);
