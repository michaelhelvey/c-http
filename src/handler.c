#include "common.h"
#include "handler.h"
#include "kqueue.h"
#include <errno.h>

handler_future_t* new_handler_future(i32 fd)
{
    handler_future_t* self = malloc(sizeof(handler_future_t));
    self->fd = fd;
    self->state = HANDLER_READING;
    self->read_buf = malloc(1024);
    self->read_buf_len = 1024;
    self->read_buf_cursor = 0;
    return self;
}

#define READ_CHUNK_SIZE 1024
#define maybe_realloc(buf, len, cursor, size)                                                      \
    if (cursor + size > len) {                                                                     \
        len *= 2;                                                                                  \
        buf = realloc(buf, len);                                                                   \
    }

async_result_t poll_read(handler_future_t* self)
{
    // check if we have enough space in the buffer:
    maybe_realloc(self->read_buf, self->read_buf_len, self->read_buf_cursor, READ_CHUNK_SIZE);

    // read from the socket:
    i32 bytes_read = read(self->fd, self->read_buf + self->read_buf_cursor, READ_CHUNK_SIZE);
    if (bytes_read == -1) {
        // 1) we errored because it would block, so we just need to re-register ourselves for the
        // next read
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            register_read_event(self->fd);
            async_result_t res = { .result = POLL_PENDING, .value = NULL };
            return res;
        }

        // 2) else it's a real error, we should return it:
        perror("read");
        async_result_t res = { .result = POLL_READY, .value = (void*)-1 };
        return res;
    }

    // otherwise we read some bytes:
    println("read %d bytes from client connection", bytes_read);
    return (async_result_t) { .result = POLL_READY, .value = (void*)(size_t)bytes_read };
}

async_result_t poll_write_buf(handler_future_t* self, char* buf, usize len)
{
    static u32 cursor = 0;
    i32 bytes_written = write(self->fd, buf + cursor, len - cursor);
    while (bytes_written != -1) {
        // we're done
        if (cursor == len) {
            async_result_t res = { .result = POLL_READY, .value = (void*)0 };
            return res;
        }

        // otherwise try again
        cursor += bytes_written;
        bytes_written = write(self->fd, buf + cursor, len - cursor);
    }

    // 1) we errored because it would block, so we just need to re-register ourselves for the write
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
        register_write_event(self->fd);
        async_result_t res = { .result = POLL_PENDING, .value = NULL };
        return res;
    }

    // else it's a real error, we should return it:
    perror("write");
    async_result_t res = { .result = POLL_READY, .value = (void*)-1 };
    return res;
}

async_result_t poll_handler_future(handler_future_t* self)
{
    switch (self->state) {
    case HANDLER_READING: {
        void* _r;
        // TODO: for now, we're only reading once...soon we will implement a stream "read until"
        // mechanism for parsing headers & body stream.
        ready(poll_read(self), _r);
        println("read request:\n%s", self->read_buf);
        self->state = HANDLER_WRITING;
    }
    case HANDLER_WRITING: {
        void* _r;
        char* buf = "HTTP/1.1 200 OK\r\nContent-Length: 12\r\n\r\nHello World!";
        ready(poll_write_buf(self, buf, strlen(buf)), _r);
        self->state = HANDLER_DONE;
    }
    default:
        return (async_result_t) { .result = POLL_READY, .value = NULL };
    }
}

void free_handler_future(handler_future_t* self)
{
    free(self->read_buf);
    free(self);
}
