#include "fs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define READ_CHUNK_SIZE 1024

static fs_read_result_t* new_read_result()
{
    fs_read_result_t* result = malloc(sizeof(fs_read_result_t));
    bzero(result, sizeof(fs_read_result_t));

    result->buffer_len = READ_CHUNK_SIZE;
    result->buffer = malloc(result->buffer_len);

    return result;
}

void free_read_result(fs_read_result_t* result)
{
    free(result->buffer);
    free(result);
}

const char* get_content_type(char* path)
{
    // FIXME: should actually just check the extension instead of full substring match
    if (strstr(path, ".html")) {
        return "text/html";
    } else if (strstr(path, ".css")) {
        return "text/css";
    } else if (strstr(path, ".js")) {
        return "application/javascript";
    } else {
        return "text/plain";
    }
}

// directory that we store our static assets in relative to CWD (without the ./)
#define DATA_DIR "/data"

fs_read_result_t* fs_read(char* path, size_t path_len)
{
    char path_str[path_len + 1];
    memcpy(path_str, path, path_len);
    path_str[path_len] = 0;

    fs_read_result_t* result = new_read_result();

    char* cwd = getcwd(NULL, 0);
    size_t cwd_len = strlen(cwd);

    // FIXME: should probably use realpath() and resolve ../../ or whatever
    size_t data_dir_len = strlen(DATA_DIR);

    char full_path[cwd_len + path_len + data_dir_len + 1];
    snprintf(full_path, sizeof(full_path), "%s%s%s", cwd, DATA_DIR, path);

    FILE* file = fopen(full_path, "r");
    if (!file) {
        free(result);
        free(cwd);
        return NULL;
    }

    size_t bytes_read = 1;
    while (bytes_read > 0) {
        if (result->content_length + READ_CHUNK_SIZE > result->buffer_len) {
            result->buffer_len *= 2;
            result->buffer = realloc(result->buffer, result->buffer_len);
        }

        bytes_read = fread(result->buffer + result->content_length, 1, READ_CHUNK_SIZE, file);

        if (bytes_read < 0) {
            perror("read");
            free(result);
            free(cwd);
            fclose(file);
            return NULL;
        }

        result->content_length += bytes_read;
    }

    fclose(file);
    free(cwd);

    // Determine content-type:
    result->content_type = get_content_type(path_str);

    return result;
}
