#include <common/util/bitmap.h>
#include <common/mm/liballoc.h>
#include <common/errno.h>

#include <string.h>

#define BITMAP_BITS 64

// Bitmap management
// Create the bitmap, with initial size
// Initial size must be a multiple of 64
int bitmap_init(struct bitmap* bitmap, size_t initial_size)
{
    if (!bitmap)
        return -EINVAL;

    // If no inital size, allocate at least 4*64 bit entries
    if (!initial_size)
        initial_size = 4;

    // Round up initial size
    if (initial_size & 0x3F)
        initial_size = (initial_size + 0x3F) & ~0x3F;

    bitmap->bitmaps = kmalloc((initial_size / BITMAP_BITS) * sizeof(uint64_t));

    if (!bitmap->bitmaps)
         // TODO: Report allocation failure
        return -ENOMEM;
    
    bitmap->bitmap_len = initial_size / BITMAP_BITS;

    memset(bitmap->bitmaps, 0, bitmap->bitmap_len * sizeof(uint64_t));

    return 0;
}

// Grows the bitmap by "grow_by" entries
int bitmap_grow(struct bitmap* bitmap, size_t grow_by)
{
    if (!bitmap)
        return -EINVAL;
    if (!grow_by)
        return -EINVAL;
    if (grow_by & 0x3F)
        grow_by = (grow_by + 0x3F) & ~0x3F;

    size_t old_len = bitmap->bitmap_len;
    size_t grow_words = grow_by / BITMAP_BITS;
    size_t new_size = bitmap->bitmap_len + grow_words;

    bitmap->bitmaps = krealloc(bitmap->bitmaps, new_size * sizeof(uint64_t));
    if (!bitmap->bitmaps)
        return -ENOMEM;

    // Zero new memory
    memset((bitmap->bitmaps + old_len), 0, grow_words * sizeof(uint64_t));
    bitmap->bitmap_len = new_size;

    return 0;
}

// Test a bit in the bitmap
// Returns -1 if the index was outside the bitmap, otherwise, the value reflecting the bit at the given bit index
int bitmap_test(struct bitmap* bitmap, size_t index)
{
    // No bitmap? no bits
    if (!bitmap)
        return -1;
    // No bitmap allocated? no bits
    if (!bitmap->bitmaps)
        return -1;

    size_t word_idx = index >> 6;
    size_t bit_idx = index & 0x3F;

    if (word_idx >= bitmap->bitmap_len)
        return -1;

    uint64_t word_bits = bitmap->bitmaps[word_idx];
    return (word_bits >> bit_idx) & 1;
}

void bitmap_set(struct bitmap* bitmap, size_t index)
{
    // No bitmap? no bits
    if (!bitmap)
        return;
    // No bitmap allocated? no bits
    if (!bitmap->bitmaps)
        return;

    size_t word_idx = index >> 6;
    size_t bit_idx = index & 0x3F;

    if (word_idx >= bitmap->bitmap_len)
        return;

    bitmap->bitmaps[word_idx] |= (1ULL << bit_idx);
}

void bitmap_clear(struct bitmap* bitmap, size_t index)
{
    // No bitmap? no bits
    if (!bitmap)
        return;
    // No bitmap allocated? no bits
    if (!bitmap->bitmaps)
        return;

    size_t word_idx = index >> 6;
    size_t bit_idx = index & 0x3F;

    if (word_idx >= bitmap->bitmap_len)
        return;

    bitmap->bitmaps[word_idx] &= ~(1ULL << bit_idx);
}
