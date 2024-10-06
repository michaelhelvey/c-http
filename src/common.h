/**
 * Global includes and aliases.
 */

#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#define VERSION "0.0.1"

// C data types can be frustratingly opaque, so we'll create some more "modern" aliases. And yes, I
// know that `int` doesn't have to be 32 bit, etc, but it is on the machines that I am targeting, so
// it's fine.
#define u16 unsigned short
#define u32 unsigned int
#define u64 unsigned long long
#define i32 int
#define i64 long long
#define usize size_t

// While this is a bit silly as it stands, eventually we can replace this with a more sophisticated
// logging system.
#define println(fmt, ...) fprintf(stdout, fmt "\n", ##__VA_ARGS__)
#define panic(fmt, ...) do { fprintf(stderr, fmt "\n", ##__VA_ARGS__); exit(1); } while (0)
