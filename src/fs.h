#pragma once

#include "common.h"

typedef struct fs_read_result_t {
    char* buffer;
    size_t buffer_len;
    size_t content_length;
    const char* content_type;
} fs_read_result_t;

// After the handler is finished transmitting the result, it should free the memory
void free_read_result(fs_read_result_t* result);

// Allocate a new read result and fill it with the file contents at <path>
fs_read_result_t* fs_read(char* path, size_t path_len);
