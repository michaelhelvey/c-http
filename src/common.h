/**
 * Global includes and aliases.
 */

#pragma once

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define VERSION "0.0.1"

// While this is a bit silly as it stands, eventually we can replace this with a more sophisticated
// logging system.
#define println(fmt, ...)                                                                          \
    do {                                                                                           \
        time_t now = time(NULL);                                                                   \
        char time_buf[20];                                                                         \
        strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", localtime(&now));                \
        fprintf(stderr, "%s: %s:%d: " fmt "\n", time_buf, __FILE__, __LINE__, ##__VA_ARGS__);      \
    } while (0)

#define panic(fmt, ...)                                                                            \
    do {                                                                                           \
        fprintf(stderr, fmt "\n", ##__VA_ARGS__);                                                  \
        exit(1);                                                                                   \
    } while (0)

// Async helpers:
typedef enum {
    POLL_PENDING,
    POLL_READY,
} poll_result_t;

typedef struct async_result_t {
    poll_result_t result;
    void* value;
} async_result_t;

#define ready(fn, result_var)                                                                      \
    do {                                                                                           \
        async_result_t res = (fn);                                                                 \
        if (res.result == POLL_PENDING)                                                            \
            return (async_result_t) { .result = POLL_PENDING, .value = NULL };                     \
        (result_var) = res.value;                                                                  \
    } while (0)
