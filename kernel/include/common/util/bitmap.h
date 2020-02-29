#ifndef __BITMAP_H__
#define __BITMAP_H__ 1

#include <common/types.h>

struct bitmap
{
    uint64_t* bitmaps;
    size_t bitmap_len;
};

// Create the bitmap, with initial size
// "initial_size" must be a multiple of 64
int bitmap_init(struct bitmap* bitmap, size_t initial_size);

// Grows the bitmap by "grow_by" entries
int bitmap_grow(struct bitmap* bitmap, size_t grow_by);

// Test a bit in the bitmap
// Returns -1 if the index was outside the bitmap, otherwise, the value reflecting the bit at the given bit index
int bitmap_test(struct bitmap* bitmap, size_t index);

void bitmap_set(struct bitmap* bitmap, size_t index);

void bitmap_clear(struct bitmap* bitmap, size_t index);

#endif /* __BITMAP_H__ */