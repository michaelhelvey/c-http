#include "common.h"
#include "kqueue.h"
#include "response.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

static void init_response_buffer(response_buffer_t* buffer)
{
    buffer->cap = 1024;
    buffer->len = 0;
    buffer->cursor = 0;
    buffer->data = malloc(buffer->cap);
}

response_t* response_new()
{
    response_t* response = malloc(sizeof(response_t));
    bzero(response, sizeof(response_t));

    response->state = RESPONSE_PREPARE;
    init_response_buffer(&response->write_buffer);
    init_response_buffer(&response->headers);

    return response;
}

void response_free(response_t* response)
{
    free(response->write_buffer.data);
    free(response->headers.data);
    // note: body expected to be freed by caller
    free(response);
}

void response_write_header_str(response_t* self, const char* key, const char* value)
{
    // 4 = space + colon + \r\n + null terminator that we will overwrite
    size_t buf_len = strlen(key) + strlen(value) + 5;
    char buf[buf_len];
    snprintf(buf, buf_len, "%s: %s\r\n", key, value);

    // make sure we have enough data in the headers buffer
    if (self->headers.cursor + buf_len > self->headers.cap) {
        self->headers.cap *= 2;
        self->headers.data = realloc(self->headers.data, self->headers.cap);
    }

    memcpy(self->headers.data + self->headers.cursor, buf, buf_len);
    self->headers.cursor += buf_len - 1;
    self->headers.len += buf_len - 1;
}

void response_write_header_int(response_t* self, const char* key, int value)
{
    // get the number of characters we need to render value:
    int num_chars = snprintf(NULL, 0, "%d", value);
    // 4 = space + colon + \r\n + the null terminator that we will overwrite
    size_t buf_len = strlen(key) + num_chars + 5;
    char buf[buf_len];
    snprintf(buf, buf_len, "%s: %d\r\n", key, value);

    // make sure we have enough data in the headers buffer
    if (self->headers.cursor + buf_len > self->headers.cap) {
        self->headers.cap *= 2;
        self->headers.data = realloc(self->headers.data, self->headers.cap);
    }

    memcpy(self->headers.data + self->headers.cursor, buf, buf_len);
    self->headers.cursor += buf_len - 1;
    self->headers.len += buf_len - 1;
}

void response_write_body(response_t* response, char* body, size_t len)
{
    response->body = body;
    response->body_len = len;
}

static void write_bytes(response_buffer_t* buffer, const char* data, size_t len)
{
    // make sure we have enough space in buffer:
    if (buffer->cursor + len > buffer->cap) {
        buffer->cap *= 2;
        buffer->data = realloc(buffer->data, buffer->cap);
    }

    memcpy(buffer->data + buffer->cursor, data, len);
    buffer->cursor += len;
    buffer->len += len;
}

static async_result_t poll_flush_write_buf(int fd, response_buffer_t* stream)
{
    int bytes_written = write(fd, stream->data + stream->cursor, stream->len - stream->cursor);
    while (bytes_written != -1) {
        stream->cursor += bytes_written;
        // we're done
        if (stream->cursor == stream->len) {
            async_result_t res = { .result = POLL_READY, .value = (void*)0 };
            return res;
        }

        // otherwise try again
        bytes_written = write(fd, stream->data + stream->cursor, stream->len - stream->cursor);
    }

    // 1) we errored because it would block, so we just need to re-register
    // ourselves for the write
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

async_result_t poll_response_write_buffer(response_t* self, int fd)
{
    while (1) {
        switch (self->state) {
        case RESPONSE_PREPARE: {
            // header line:
            write_bytes(&self->write_buffer, "HTTP/1.1 ", 9);
            write_bytes(&self->write_buffer, self->status_code, strlen(self->status_code));
            write_bytes(&self->write_buffer, " ", 1);
            write_bytes(&self->write_buffer, self->status_text, strlen(self->status_text));
            write_bytes(&self->write_buffer, "\r\n", 2);

            if (self->headers.len > 0) {
                // headers:
                write_bytes(&self->write_buffer, self->headers.data, self->headers.cursor);
            }
            write_bytes(&self->write_buffer, "\r\n", 2);

            if (self->body_len > 0) {
                write_bytes(&self->write_buffer, self->body, self->body_len);
            }

            self->write_buffer.cursor = 0;
            self->state = RESPONSE_POLLING;
            break;
        }
        case RESPONSE_POLLING: {
            void* r;
            ready(poll_flush_write_buf(fd, &self->write_buffer), r);
            long ret_val = (long)r;
            if (ret_val < 0) {
                return (async_result_t) { .result = POLL_READY, .value = (void*)-1 };
            }
            self->state = RESPONSE_DONE;
            break;
        }
        case RESPONSE_DONE: {
            return (async_result_t) { .result = POLL_READY, .value = (void*)0 };
        }
        default:
            assert(0 && "unreachable");
        }
    }
}
