#include "arena.h"
#include "common.h"
#include "handler.h"
#include "kqueue.h"
#include <assert.h>
#include <errno.h>
#include <stdalign.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define HEADERS_ARENA_SIZE 4096

bool string_view_equals(string_view_t a, string_view_t b)
{
    if (a.len != b.len) {
        return false;
    }
    return strncmp(a.data, b.data, a.len) == 0;
}

static void init_handler_future(handler_future_t* self, int fd)
{
    self->fd = fd;
    self->state = HANDLER_READING_HEADERS;

    if (self->arena) {
        arena_release(self->arena);
    }
    self->arena = arena_create(HEADERS_ARENA_SIZE);

    self->request = (request_t) {
        .method = { .data = NULL, .len = 0 },
        .path = { .data = NULL, .len = 0 },
        .version = { .data = NULL, .len = 0 },
        .content_length = 0,
        .headers = NULL,
    };

    if (self->read_stream.data) {
        free(self->read_stream.data);
    }

    self->read_stream = (read_stream_t) {
        .data = malloc(1024),
        .len = 1024,
        .write_cursor = 0,
        .read_cursor = 0,
        .body_start_idx = 0,
    };
    bzero(self->read_stream.data, 1024);
    self->write_stream = NULL;
}

handler_future_t* new_handler_future(int fd)
{
    handler_future_t* self = malloc(sizeof(handler_future_t));
    bzero(self, sizeof(handler_future_t));
    init_handler_future(self, fd);
    return self;
}

void free_handler_future(handler_future_t* self)
{
    arena_release(self->arena);
    free(self->read_stream.data);
    free(self);
}

static void init_write_stream(handler_future_t* self, char* buf, size_t len)
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

static async_result_t poll_read(int fd, read_stream_t* stream)
{
    // check if we have enough space in the buffer:
    maybe_realloc(stream->data, stream->len, stream->write_cursor, READ_CHUNK_SIZE);

    // read from the socket:
    int bytes_read = read(fd, stream->data + stream->write_cursor, READ_CHUNK_SIZE);
    println("poll_read: bytes_read = %d", bytes_read);
    if (bytes_read == -1) {
        // 1) we errored because it would block, so we just need to re-register
        // ourselves for the next read
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

    // if bytes_read == 0, I'm assuming that means that the connection is closed, and we should
    // just return an error so we get reset.
    if (bytes_read == 0) {
        println("error: client %d closed connection", fd);
        async_result_t res = { .result = POLL_READY, .value = (void*)-1 };
        return res;
    }

    // otherwise we read some bytes:
    println("read %d bytes from client connection", bytes_read);
    stream->write_cursor += bytes_read;
    return (async_result_t) { .result = POLL_READY, .value = (void*)(size_t)bytes_read };
}

static int max(int a, int b) { return a > b ? a : b; }

static int boyer_moore_search(char* haystack, char* needle)
{
    int m = strlen(needle);
    int n = strlen(haystack);

    int bad_char[256];
    for (int i = 0; i < 256; i++) {
        bad_char[i] = -1;
    }

    for (int i = 0; i < m; i++) {
        bad_char[(int)needle[i]] = i;
    }

    int s = 0;
    while (s <= n - m) {
        int j = m - 1;
        while (j >= 0 && needle[j] == haystack[s + j]) {
            j--;
        }

        if (j < 0) {
            return s;
        }

        s += max(1, j - bad_char[(int)haystack[s + j]]);
    }

    return -1;
}

static async_result_t poll_read_headers(handler_future_t* self)
{
    char* haystack = self->read_stream.data + self->read_stream.read_cursor;
    char* needle = "\r\n\r\n";

    int found = boyer_moore_search(haystack, needle);

    while (found == -1) {
        // we haven't found them, so forward the cursor and poll for more data
        self->read_stream.read_cursor = self->read_stream.write_cursor;
        void* r;
        ready(poll_read(self->fd, &self->read_stream), r);

        // if we errored, return the error
        long return_val = (long)r;
        if (return_val == -1) {
            return (async_result_t) { .result = POLL_READY, .value = (void*)-1 };
        }

        // otherwise, we have read some bytes, so we should search again
        found = boyer_moore_search(haystack, needle);
    }

    // We found it
    self->read_stream.body_start_idx = found + 4;
    println("read to end of headers, body starts at %ld", self->read_stream.body_start_idx);
    // Reset our read_cursor so our parser can read from the beginning
    self->read_stream.read_cursor = 0;
    return (async_result_t) { .result = POLL_READY, .value = (void*)0 };
}

static async_result_t poll_flush_write_buf(int fd, write_stream_t* stream)
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

static header_t* get_header(request_t* request, char* key)
{
    header_t* current = request->headers;
    while (current) {
        if (strncmp(current->key.data, key, current->key.len) == 0) {
            return current;
        }
        current = current->next;
    }

    return NULL;
}

// Gets the content-length of the body. Returns 0 is there is no body, and -1 if there is an error
// in parsing the header.
static long get_content_length(request_t* request)
{
    if (request->content_length != 0) {
        return request->content_length;
    }

    header_t* header = get_header(request, "Content-Length");
    if (!header) {
        return 0;
    }

    assert(header->value.next == NULL && "multiple Content-Length header values are invalid");

    // enough space for a cstr of the value
    char str_buf[header->value.name.len + 1];
    memcpy(str_buf, header->value.name.data, header->value.name.len);
    str_buf[header->value.name.len] = '\0';

    long r;
    if ((r = strtol(str_buf, NULL, 10)) == 0) {
        perror("strtol: unable to parse Content-Length header value");
        return -1;
    }

    request->content_length = r;

    return r;
}

static void insert_header(request_t* request, header_t* header)
{
    if (!request->headers) {
        request->headers = header;
        return;
    }

    header_t* current = request->headers;
    header_t* tail = NULL;
    while (current) {
        if (string_view_equals(current->key, header->key)) {
            // TODO: look up what the expected behavior is supposed to be if we encounter the same
            // value as well...probably just a no-op? Either way this is bad behavior on the part of
            // the client if that happens.

            // forward down the linked list of values until we get to the end:
            header_value_t* value = &current->value;
            while (value->next) {
                value = value->next;
            }

            value->next = &header->value;
            return;
        }
        tail = current;
        current = current->next;
    }

    tail->next = header;
}

// At this point we know know that everything up to the body has been read, so
// we can parse the request line and headers into the request struct that we
// own.
static void parse_request(handler_future_t* self)
{
    request_t* request = &self->request;
    read_stream_t* stream = &self->read_stream;
    assert(stream->body_start_idx && "parse_request called before headers were read");

    // request-line: method SP request-target SP HTTP-version CRLF
    request->method.data = stream->data;
    for (size_t i = 0; i < stream->body_start_idx; i++) {
        if (stream->data[stream->read_cursor] == ' ') {
            request->method.len = i;
            stream->read_cursor++;
            break;
        }
        stream->read_cursor++;
    }

    request->path.data = stream->data + stream->read_cursor;
    for (size_t i = 0; i < stream->body_start_idx; i++) {
        if (stream->data[stream->read_cursor] == ' ') {
            request->path.len = i;
            stream->read_cursor++;
            break;
        }

        stream->read_cursor++;
    }

    request->version.data = stream->data + stream->read_cursor;
    for (size_t i = 0; i < stream->body_start_idx; i++) {
        if (stream->data[stream->read_cursor] == '\r') {
            request->version.len = i;
            stream->read_cursor++;
            break;
        }
        stream->read_cursor++;
    }

    // skip the last \n of the CRLF
    stream->read_cursor++;

    // TODO: what if we have no headers at all?
    while (stream->read_cursor < stream->body_start_idx) {
        header_t* current_header = arena_alloc(self->arena, sizeof(header_t), alignof(header_t));
        bzero(current_header, sizeof(header_t));

        // Parse the header key
        current_header->key.data = stream->data + stream->read_cursor;
        for (size_t i = 0; i < stream->body_start_idx; i++) {
            if (stream->data[stream->read_cursor] == ':') {
                current_header->key.len = i;
                stream->read_cursor++;
                break;
            }
            stream->read_cursor++;
        }

        // Skip the optional whitespace after the header
        if (stream->data[stream->read_cursor] == ' ') {
            stream->read_cursor++;
        }

        // Parse the header value
        current_header->value.name.data = stream->data + stream->read_cursor;
        for (size_t i = 0; i < stream->body_start_idx; i++) {
            if (stream->data[stream->read_cursor] == '\r') {
                current_header->value.name.len = i;
                stream->read_cursor++;
                break;
            }
            stream->read_cursor++;
        }

        // Insert the header into the request
        insert_header(request, current_header);

        // skip the CRLF:
        stream->read_cursor++;

        // Check if there are more headers to read:
        if (stream->data[stream->read_cursor] == '\r') {
            stream->read_cursor += 2;
            break;
        }
    }
}

static void pretty_print_request(handler_future_t* self)
{
    request_t* request = &self->request;
    printf("method=");
    fwrite(request->method.data, 1, request->method.len, stdout);

    printf(" path=");
    fwrite(request->path.data, 1, request->path.len, stdout);

    printf(" version=");
    fwrite(request->version.data, 1, request->version.len, stdout);

    printf(" headers=");

    printf("{");
    header_t* curr = request->headers;
    while (curr) {
        printf("\"");
        fwrite(curr->key.data, 1, curr->key.len, stdout);
        printf("\": \"");
        // Iterate over the value linked list and print as comma-separated values:
        header_value_t* value = &curr->value;
        while (value) {
            fwrite(value->name.data, 1, value->name.len, stdout);
            value = value->next;
            if (value) {
                printf(", ");
            }
        }
        printf("\"");
        curr = curr->next;

        if (curr) {
            printf(", ");
        }
    }
    printf("} body=\"");

    if (request->content_length > 0) {
        fwrite(self->read_stream.data + self->read_stream.body_start_idx, 1,
            request->content_length, stdout);
    }

    printf("\"\n");
}

async_result_t poll_read_body(handler_future_t* self)
{
    size_t body_start = self->read_stream.body_start_idx;
    long content_length = get_content_length(&self->request);

    if (content_length == -1) {
        return (async_result_t) { .result = POLL_READY, .value = (void*)-1 };
    }

    if (content_length == 0) {
        return (async_result_t) { .result = POLL_READY, .value = (void*)0 };
    }

    // How far we need to read the write_cursor up to ensure that the whole body is in
    // our buffer:
    size_t read_up_to = body_start + content_length;

    while (self->read_stream.write_cursor < read_up_to) {
        void* _r;
        ready(poll_read(self->fd, &self->read_stream), _r);
        long ret_val = (long)_r;
        if (ret_val < 0) {
            return (async_result_t) { .result = POLL_READY, .value = (void*)-1 };
        }
        self->read_stream.write_cursor += ret_val;
    }

    // At this point the whole body is buffered in our read stream:
    return (async_result_t) { .result = POLL_READY, .value = (void*)0 };
}

async_result_t poll_handler_future(handler_future_t* self)
{
    while (1) { // will be broken by the return statements in each state
        switch (self->state) {
        case HANDLER_READING_HEADERS: {
            void* _r;
            ready(poll_read_headers(self), _r);
            long ret_val = (long)_r;
            if (ret_val < 0) {
                return (async_result_t) { .result = POLL_READY, .value = (void*)-1 };
            }
            parse_request(self);
            self->state = HANDLER_READING_BODY;
            break;
        }
        case HANDLER_READING_BODY: {
            void* _r;
            ready(poll_read_body(self), _r);
            long ret_val = (long)_r;
            if (ret_val < 0) {
                return (async_result_t) { .result = POLL_READY, .value = (void*)-1 };
            }
            pretty_print_request(self);
            self->state = HANDLER_WRITING;
            break;
        }
        case HANDLER_WRITING: {
            void* _r;
            char* buf = "HTTP/1.1 200 OK\r\nContent-Length: 12\r\n\r\nHello World!";
            init_write_stream(self, buf, strlen(buf));
            ready(poll_flush_write_buf(self->fd, self->write_stream), _r);
            self->state = HANDLER_DONE;
            break;
        }
        case HANDLER_DONE: {
            // Now that we are done, we need to reset our state so that we can
            // handle the next request on the connection (if there is one):
            // TODO: I think this is probably kinda sloppy, and we should probably be re-using the
            // buffers, but for now I think it's easiest to just reset everything for each handler.
            init_handler_future(self, self->fd);
            return (async_result_t) { .result = POLL_READY, .value = (void*)HANDLER_KEEPALIVE };
        }
        default: {
            assert(0 && "unreachable");
        }
        }
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

#define test_assert(cond, msg)                                                                     \
    do {                                                                                           \
        if (!(cond)) {                                                                             \
            printf("Assertion failed: %s\n", msg);                                                 \
            return -1;                                                                             \
        }                                                                                          \
    } while (0)

static int test_header_insertion()
{
    // Scenario 1: inserting a header into an empty request
    char* buf = "Accept: text/html";
    request_t request;
    request.headers = NULL;
    header_t header;
    bzero(&header, sizeof(header_t));

    header.key.data = buf;
    header.key.len = 6;

    header.value.name.data = buf + 8;
    header.value.name.len = 9;

    insert_header(&request, &header);
    test_assert(strncmp(request.headers->key.data, "Accept", request.headers->key.len) == 0,
        "header key should be Accept");
    test_assert(get_header(&request, "Accept") == &header,
        "expected get_header to return pointer to original header");

    // Scenario 2: inserting another header into the request
    char* buf2 = "Content-Type: text/plain";
    header_t header2;
    bzero(&header2, sizeof(header_t));
    header2.key.data = buf2;
    header2.key.len = 12;

    header2.value.name.data = buf2 + 14;
    header2.value.name.len = 10;

    insert_header(&request, &header2);
    test_assert(get_header(&request, "Content-Type") == &header2,
        "expected get_header to return pointer to original header");

    // Scenario 3: inserting a header that already exists (array of values)
    char* buf3 = "Content-Type: something-else";
    header_t header3;
    bzero(&header3, sizeof(header_t));
    header3.key.data = buf3;
    header3.key.len = 12;

    header3.value.name.data = buf2 + 14;
    header3.value.name.len = 14;

    insert_header(&request, &header3);
    test_assert(get_header(&request, "Content-Type") == &header2,
        "expected get_header to return pointer to original header");
    test_assert(header2.value.next == &header3.value,
        "expected insert_header to place additonal values in header2 linked list");

    return 0;
}

static int test_string_search()
{
    char* haystack = "hello world";
    char* needle = "world";
    test_assert(
        boyer_moore_search(haystack, needle) == 6, "boyer_moore_search(haystack, needle) == 6");

    haystack = "hello world";
    needle = "hello";
    test_assert(
        boyer_moore_search(haystack, needle) == 0, "boyer_moore_search(haystack, needle) == 0");

    haystack = "hello world";
    needle = "not found";
    test_assert(
        boyer_moore_search(haystack, needle) == -1, "boyer_moore_search(haystack, needle) == -1");
    return 0;
}

int handler_test_suite()
{
    int r = 0;
    if (test_string_search() < 0) {
        r = -1;
        printf("\t❌ test_string_search\n");
    } else {
        printf("\t✅ test_string_search\n");
    }
    if (test_header_insertion() < 0) {
        r = -1;
        printf("\t❌ test_header_insertion\n");
    } else {
        printf("\t✅ test_header_insertion\n");
    }
    return r;
}
