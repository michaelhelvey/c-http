#include "arena.h"
#include "common.h"
#include "handler.h"
#include "kqueue.h"
#include <errno.h>
#include <stdalign.h>

bool string_view_equals(string_view_t a, string_view_t b)
{
    if (a.len != b.len) {
        return false;
    }
    return strncmp(a.data, b.data, a.len) == 0;
}

handler_future_t* new_handler_future(int fd)
{
    handler_future_t* self = malloc(sizeof(handler_future_t));
    self->fd = fd;
    self->state = HANDLER_READING;
    self->arena = arena_create(1024);
    self->request = (request_t) {
        .method = { .data = NULL, .len = 0 },
        .path = { .data = NULL, .len = 0 },
        .version = { .data = NULL, .len = 0 },
        .headers = NULL,
    };
    self->read_stream = (read_stream_t) {
        .data = malloc(1024),
        .len = 1024,
        .write_cursor = 0,
        .read_cursor = 0,
    };
    self->write_stream = NULL;
    return self;
}

void free_handler_future(handler_future_t* self)
{
    arena_release(self->arena);
    free(self->read_stream.data);
    free(self);
}

void init_write_stream(handler_future_t* self, char* buf, size_t len)
{
    write_stream_t* stream
        = arena_alloc(self->arena, sizeof(write_stream_t), alignof(write_stream_t));
    stream->data = buf;
    stream->len = len;
    stream->cursor = 0;
    self->write_stream = stream;
}

#define READ_CHUNK_SIZE 1024
#define maybe_realloc(buf, len, cursor, size)                                                      \
    if (cursor + size > len) {                                                                     \
        len *= 2;                                                                                  \
        buf = realloc(buf, len);                                                                   \
    }

async_result_t poll_read(int fd, read_stream_t* stream)
{
    // check if we have enough space in the buffer:
    maybe_realloc(stream->data, stream->len, stream->write_cursor, READ_CHUNK_SIZE);

    // read from the socket:
    int bytes_read = read(fd, stream->data + stream->write_cursor, READ_CHUNK_SIZE);
    if (bytes_read == -1) {
        // 1) we errored because it would block, so we just need to re-register ourselves for the
        // next read
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            register_read_event(fd);
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
    stream->write_cursor += bytes_read;
    return (async_result_t) { .result = POLL_READY, .value = (void*)(size_t)bytes_read };
}

// The next challenge we have to face is that we need to know where in the request we've parsed up
// to before we call poll_read again. I think that the best way to do this is to read until we get
// the double new line that means the headers are done, and to do this before we even start parsing.
// So we have the following states 1) reading headers 2) parsing request (not really a state, we can
// just do it sync after reading the headers) 3) reading body (?) 4) writing

// TODO: implement request parsing
// async_result_t poll_parse_request(handler_future_t* self)
// {
//     println("parsing request: %ld", self->stream_cursor);
//     // Advance the read buffer until we have read the request line and the headers, and
//     // stream_cursor rests at the beginning of the body.  At this point all the string views etc
//     // in the request struct should be pointing to the correct locations in the read buffer.

//     // I think maybe I want something like this?
//     // void* request_line;
//     // ready(poll_parse_request_line(self), request_line);

//     return (async_result_t) { .result = POLL_READY, .value = NULL };
// }

async_result_t poll_flush_write_buf(int fd, write_stream_t* stream)
{
    int bytes_written = write(fd, stream->data + stream->cursor, stream->len - stream->cursor);
    while (bytes_written != -1) {
        // we're done
        if (stream->cursor == stream->len) {
            async_result_t res = { .result = POLL_READY, .value = (void*)0 };
            return res;
        }

        // otherwise try again
        stream->cursor += bytes_written;
        bytes_written = write(fd, stream->data + stream->cursor, stream->len - stream->cursor);
    }

    // 1) we errored because it would block, so we just need to re-register ourselves for the write
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
        register_write_event(fd);
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
        ready(poll_read(self->fd, &self->read_stream), _r);
        println("read request:\n%s", self->read_stream.data);
        self->state = HANDLER_WRITING;
    }
    case HANDLER_WRITING: {
        void* _r;
        char* buf = "HTTP/1.1 200 OK\r\nContent-Length: 12\r\n\r\nHello World!";
        init_write_stream(self, buf, strlen(buf));
        ready(poll_flush_write_buf(self->fd, self->write_stream), _r);
        self->state = HANDLER_DONE;
    }
    default: {
        return (async_result_t) { .result = POLL_READY, .value = (void*)HANDLER_CLOSE };
    }
    }
}
