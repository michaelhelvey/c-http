#include "handler.h"

handler_future_t* new_handler_future(i32 fd)
{
    handler_future_t* self = malloc(sizeof(handler_future_t));
    self->fd = fd;
    self->state = HANDLER_READING;
    self->read_buf = malloc(1024);
    self->read_buf_len = 1024;
    return self;
}

async_result_t poll_handler_future(handler_future_t* self)
{
    // TODO: make this more real
    println("Polling handler future %d, state = %d", self->fd, self->state);
    async_result_t res = { .result = POLL_PENDING, .value = NULL };
    res.result = POLL_READY;
    res.value = "hello world";
    return res;
}

void free_handler_future(handler_future_t* self)
{
    free(self->read_buf);
    free(self);
}
