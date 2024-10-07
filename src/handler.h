#pragma once

#include "common.h"

typedef enum handler_future_state_t {
    HANDLER_READING,
    HANDLER_WRITING,
    HANDLER_DONE,
} handler_future_state_t;

typedef struct handler_future_t {
    i32 fd;
    handler_future_state_t state;
    char* read_buf;
    usize read_buf_len;
    usize read_buf_cursor;
} handler_future_t;

handler_future_t* new_handler_future(i32 fd);

async_result_t poll_handler_future(handler_future_t* self);

void free_handler_future(handler_future_t* self);
