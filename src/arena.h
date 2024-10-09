/*
This module contains our basic memory allocation strategy for requests.  It works as follows:

1) For request headers, we allocate a linked list of relatively small regions (1k or 4k bytes) to
contain header data, request metadata, scratch memory, etc.

2) For the read buffer that we read from the client file descriptor, we use `malloc` to allocate
(and potentially re-allocate as necessary) all the memory to hold the request. It's not so useful
to use an arena allocator for this as we require a single continuous block, rather than a linked
list of regions.

Note that we currently don't currently support any kind of streaming or chunked encoding where we
re-use the same buffer for different parts of the request body (though we could add this in the
future).
*/

#pragma once

#include "common.h"

typedef struct region_t {
    // Address of the start of the region in memory
    uintptr_t start;
    // The total length of the region
    usize len;
    // The first available free address in the region
    uintptr_t free_cursor;
    // A pointer to the next region in the arena's linked list
    struct region_t* next;
} region_t;

typedef struct arena_t {
    // The first region in the arena
    region_t* head;
    // The current region we are allocating from
    region_t* current;
    // The size of each region in bytes
    usize region_size;
    // The number of regions we have allocated so far
    usize region_count;
} arena_t;

// Allocates a new arena with a single region of the given size.
arena_t* arena_create(usize region_size);

// Allocates a new chunk of memory in the arena's current region with the given size and alignment
void* arena_alloc(arena_t* arena, usize size, usize align);

// Releases all memory allocated by the arena, including the arena itself
void arena_release(arena_t* arena);

i32 arena_test_suite();
