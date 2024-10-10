#pragma once

#include "common.h"

typedef enum response_write_state_t {
    RESPONSE_PREPARE,
    RESPONSE_POLLING,
    RESPONSE_DONE,
} response_write_state_t;

// The actual buffer of data that we write to a file descriptor
typedef struct response_buffer_t {
    // allocated memory to write to
    char* data;
    // the total allocated length of `data`
    size_t cap;
    // the actual length of the real data in the buffer
    size_t len;
    // variable cursor into the buffer tracking how much data we have written
    size_t cursor;
} response_buffer_t;

typedef struct response_t {
    response_write_state_t state;
    response_buffer_t write_buffer;
    // note: this is kinda cheating but I just don't want to have to bother writing a hashmap right
    // now
    response_buffer_t headers;
    char* body;
    size_t body_len;
    const char* status_code;
    const char* status_text;
} response_t;

response_t* response_new();
void response_free(response_t* response);

void response_write_header_str(response_t* response, const char* key, const char* value);
void response_write_header_int(response_t* self, const char* key, int value);
void response_write_body(response_t* response, char* body, size_t len);

async_result_t poll_response_write_buffer(response_t* response, int fd);
