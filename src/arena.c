#include "arena.h"

// Aligns a pointer `addr` up to the closest address that is aligned with `align`, assuming of
// course that `align` is a power of two.
#define align_up(addr, align) ((addr) + ((align - 1)) & ~((align - 1)))

#define ALLOC_DEBUG 0
#define debug_alloc(fmt, ...)                                                                      \
    do {                                                                                           \
        if (ALLOC_DEBUG) {                                                                         \
            printf(fmt, ##__VA_ARGS__);                                                            \
        }                                                                                          \
    } while (0)

static region_t* region_create(usize size)
{
    region_t* region = malloc(sizeof(region_t));
    region->start = (uintptr_t)malloc(size);
    region->len = size;
    region->free_cursor = region->start;
    region->next = NULL;
    return region;
}

arena_t* arena_create(usize region_size)
{
    region_t* initial_region = region_create(region_size);
    arena_t* arena = malloc(sizeof(arena_t));

    arena->region_size = region_size;
    arena->region_count = 1;
    arena->head = initial_region;
    arena->current = initial_region;

    return arena;
}

void* arena_alloc(arena_t* arena, usize size, usize align)
{
    // Check that the size is not zero & is not larger than a region
    if (size == 0 || size > arena->region_size) {
        return NULL;
    }

    // Align the cursor to the given alignment
    uintptr_t aligned_cursor = align_up(arena->current->free_cursor, align);

    // Calculate the amount of free space left in the current region
    usize bytes_used = aligned_cursor - arena->current->start;
    usize current_free = arena->current->len - bytes_used;

    debug_alloc("allocating %zu bytes, current region bytes used = %zu, current free = %zu\n", size,
        bytes_used, current_free);

    // Check if we need to allocate a new region
    if (size > current_free) {
        debug_alloc("allocating new region because %zu requested is greater than %zu free space\n",
            size, current_free);
        region_t* new_region = region_create(arena->region_size);
        arena->current->next = new_region;
        arena->current = new_region;
        arena->region_count++;
        aligned_cursor = align_up(arena->current->free_cursor, align);
    }

    // Allocate the memory
    uintptr_t new_cursor = aligned_cursor + size;
    arena->current->free_cursor = new_cursor;

    return (void*)aligned_cursor;
}

void arena_release(arena_t* arena)
{
    region_t* current = arena->head;
    while (current != NULL) {
        region_t* next = current->next;
        free((void*)current->start);
        free(current);
        current = next;
    }
    free(arena);
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

#define assert(cond, msg)                                                                          \
    do {                                                                                           \
        if (!(cond)) {                                                                             \
            printf("Assertion failed: %s\n", msg);                                                 \
            return -1;                                                                             \
        }                                                                                          \
    } while (0)

i32 test_arena_alloc()
{
    // Allocate an arena with a region size of 12 bytes
    arena_t* arena = arena_create(12);
    assert(arena != NULL, "arena_create failed");

    // Allocate all 12 bytes:
    void* ptr1 = arena_alloc(arena, 12, 1);
    assert(ptr1 != NULL, "arena_alloc failed");
    assert(arena->region_count == 1, "12 bytes should fit in one region");

    // Allocate 4 more bytes, should go into the second region
    void* ptr2 = arena_alloc(arena, 4, 1);
    assert(ptr2 != NULL, "arena_alloc failed");
    assert(arena->region_count == 2, "16 bytes should fit in two regions");

    arena_release(arena);

    return 0;
}

i32 arena_test_suite()
{
    i32 r = 0;
    if (test_arena_alloc() < 0) {
        r = -1;
        printf("\t❌ test_arena_alloc\n");
    } else {
        printf("\t✅ test_arena_alloc\n");
    }
    return r;
}
