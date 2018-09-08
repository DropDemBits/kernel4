/**
 * Copyright (C) 2018 DropDemBits
 * 
 * This file is part of Kernel4.
 * 
 * Kernel4 is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * Kernel4 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with Kernel4.  If not, see <http://www.gnu.org/licenses/>.
 * 
 */

#include <common/mm/mm.h>
#include <common/util/kfuncs.h>

extern size_t kernel_phystart;
extern size_t kernel_physize;
extern void* mm_get_base();

enum {
    RTYPE_LOW,
    RTYPE_32BIT,
    RTYPE_64BIT,
};

struct mem_flags
{
    uint64_t present : 1;
    uint64_t type : 2;
    uint64_t reserved : 9;
} __attribute__((__packed__));
typedef struct mem_flags mem_flags_t;

struct mem_block
{
    struct mem_block* next;
    uint64_t* bitmap;
    uint64_t super_map;

    uint64_t base : 48;
    mem_flags_t flags;
} __attribute__((__packed__));
typedef struct mem_block mem_region_t;

static mem_region_t* region_list = KNULL;
static size_t total_blocks = 1;
static size_t frame_blocks = 1;

// List utilities (TODO: Abstract away into a new file)
/*
 * Gets the last item in the list
 */
static mem_region_t* list_get_tail(mem_region_t* list_head)
{
    mem_region_t* last_item = list_head;

    if(last_item == KNULL) return KNULL;
    while(last_item->next != KNULL) last_item = (mem_region_t*)last_item->next;

    return last_item;
}

/*
 * Inserts a block after the specified item in a list
 */
static void list_insert_after(mem_region_t* list_head, mem_region_t* insert)
{
    if(list_head == KNULL || insert == KNULL) return;
    // Are we inserting at the list tail?
    if(list_head->next == KNULL)
    {
        // Append item to list tail
        list_head->next = insert;
        return;
    }

    // Insert between two items
    mem_region_t* next_item = list_head->next;
    list_head->next = insert;
    /*
     * We are trusting that the insert provided is safe. Can this
     * assumption bring about a security flaw?
     */
    list_get_tail(insert)->next = next_item;
}

/*
 * Searches list for a memory block which contains the address
 * Returns the block which matches the requirements, or KNULL if none was found
 */
static mem_region_t* list_search_block(mem_region_t* list_head, uint64_t addr)
{
    // TODO: Abstract away into a macro
    mem_region_t* block = list_head;

    while(block != KNULL)
    {
        if(addr >= block->base) break;
        block = block->next;
    }

    return block;
}

/*
 * Searches list for the first free memory block
 * Returns the first free block, or KNULL if none was found
 */
static mem_region_t* list_search_free(mem_region_t* list_head)
{
    // TODO: Abstract away into a macro
    mem_region_t* block = list_head;

    while(block != KNULL)
    {
        if(block->super_map != 0xFFFFFFFFFFFFFFFFULL && block->flags.present)
            break;
        block = block->next;
    }

    return block;
}

/*
 * Appends a new block to the list. Automatically allocates frames to the list
 * if needed.
 * Returns true if a new pointer block was allocated
 */
static bool mm_append_block(mem_region_t* list_head, mem_region_t* block)
{
    list_insert_after(list_head, block);
    total_blocks++;
    if(++frame_blocks >= 64)
    {
        block->next = (struct mem_block*)mm_alloc(1);
        return true;
    }
    return false;
}

// Bitmap utilities
typedef union
{
    struct
    {
        uint16_t bit_index : 6;
        uint16_t qword_index : 9;
    };
    uint16_t value;
} bm_index_t;

typedef union
{
    struct
    {
        uint16_t padding : 9;
        uint16_t bit_index : 6;
    };
    uint16_t value;
} sbm_index_t;

static void bm_set_bit(mem_region_t* mem_block, size_t index)
{
    if(mem_block == KNULL) return;
    if(index >= 32768)
    {
        // TODO: Alert our things
        return;
    }

    bm_index_t bm_index = {.value = index};
    sbm_index_t sbm_index = {.value = index};

    mem_block->bitmap[bm_index.qword_index] |= (1ULL << bm_index.bit_index);

    // Check superblocks
    bool sbi_full = true;
    for(uint16_t i = 0; i < 0b111; i++)
    {
        if(mem_block->bitmap[(bm_index.qword_index & ~0x7ULL) + i] != ~0ULL)
            return;
    }

    if(sbi_full) mem_block->super_map |= (1ULL << sbm_index.bit_index);
}

static void bm_clear_bit(mem_region_t* mem_block, size_t index)
{
    if(mem_block == KNULL) return;
    if(index >= 32768)
    {
        // TODO: Alert our things
        return;
    }

    bm_index_t bm_index = {.value = index};
    sbm_index_t sbm_index = {.value = index};

    mem_block->bitmap[bm_index.qword_index] &= ~(1ULL << bm_index.bit_index);
    mem_block->super_map &= ~(1ULL << sbm_index.bit_index);
}

static uint8_t bm_get_bit(mem_region_t* mem_block, size_t index)
{
    if(mem_block == KNULL) return 1;
    if(index >= 32768)
    {
        // TODO: Alert our things
        return 1;
    }

    bm_index_t bm_index = {.value = index};
    return (uint8_t) (mem_block->bitmap[bm_index.qword_index] >> (bm_index.bit_index)) & 0x1;
}

static uint8_t bm_test_bit(uint64_t bitmap, size_t index)
{
    if(index > 64) return 1;
    return (uint8_t) ((bitmap >> index) & 0x1);
}

static size_t bm_find_free_bits(mem_region_t* mem_block, size_t size)
{
    bm_index_t index = {.value = 0xFFFF};
    sbm_index_t sbm_index = {.value = 0x0000};
    size_t num_free_bits = 0;

    for(size_t sb_index = 0; sb_index < 64; sb_index++)
    {
        if(bm_test_bit(mem_block->super_map, sb_index) == 0)
        {
            sbm_index.bit_index = sb_index;
            index.value = sbm_index.bit_index;

            for(size_t i = 0; i < 512; i++)
            {
                index.bit_index = i & 0x3F;
                index.qword_index = (i >> 6) | (sbm_index.value >> 6);

                if(bm_get_bit(mem_block, index.value) == 0)
                {
                    if(++num_free_bits >= size) break;
                } else
                {
                    num_free_bits = 0;
                }
            }
        }

        if(num_free_bits == 0) index.value = 0xFFFF;
        else if(num_free_bits >= size) break;
    }

    return (size_t)index.value;
}

/*
 * Creates a region block and appends it to list_head.
 * Base to be passed should be base >> 27 (size of region coverage).
 */
static mem_region_t* mm_create_region(mem_region_t* list_head,
                                     uint64_t base)
{
    mem_region_t* append = KNULL;
    if(list_head->next == KNULL || list_head->next == (void*)~0ULL)
    {
        append = (mem_region_t*)((size_t)list_head + sizeof(mem_region_t));
    } else
    {
        append = list_head->next;
    }

    append->next = KNULL;
    append->base = base;
    append->super_map = 1ULL;

    if(base == 0x0) append->flags.type = RTYPE_LOW;
    else if(base <= 32) append->flags.type = RTYPE_32BIT;
    else append->flags.type = RTYPE_64BIT;
    append->flags.reserved = 0xFFU;
    append->flags.present = 0;

    append->bitmap = (uint64_t*)mm_alloc(1);
    for(size_t i = 0; i < 512 && append->bitmap != KNULL; i++)
    {
        append->bitmap[i] = ~0ULL;
    }

    append->flags.present = 1;
    mm_append_block(list_head, append);
    return list_head->next;
}

void mm_init()
{
    heap_init();

    // Now that we have our regions defined, map our region list to virt-mem
    if(region_list != KNULL)
    {
        // Pass1: Next pointers

        mem_region_t* block = region_list;
        mem_region_t* next_pointer = mm_get_base();

        mmu_map_direct((unsigned long)next_pointer, (unsigned long)region_list);
        region_list = mm_get_base();
        next_pointer++;

        while(block->next != KNULL)
        {
            mmu_map_direct((unsigned long)next_pointer, (unsigned long)block->next);
            block->next = next_pointer++;
            block = block->next;
        }

        // Pass2: Bitmaps
        uint64_t *block_pointer = (uint64_t*) (((uintptr_t)next_pointer + 0xFFF) & ~0xFFF);
        block = region_list;
        while(block != KNULL)
        {
            mmu_map_direct((unsigned long)block_pointer, (unsigned long)block->bitmap);
            block->bitmap = block_pointer;

            block_pointer += (0x1000 >> 3);
            block = block->next;
        }

    }
}

void mm_add_region(unsigned long base, size_t length, uint32_t type)
{
    mem_region_t *tail = list_get_tail(region_list);
    bool cover_kernel = false;

    if(tail == KNULL)
    {
        bool base_in_bda = false;
        cover_kernel = true;

        // Set first 8K block in region for region tracking
        if(base < 0x1000)
        {
            // On PC-BIOS systems, the low 512B contains information we still
            // need, so we don't overwrite the memory yet.
            base += 0x1000;
            length -= 0x1000;
            base_in_bda = true;
        }
        region_list = (mem_region_t*)base;

        base += 0x2000;
        length -= 0x2000;

        //Set region to 0xFF
        for(size_t i = 0; i < 2048; i++)
        {
            ((uint32_t*)region_list)[i] = 0xFFFFFFFF;
        }

        region_list->next = KNULL;
        region_list->base = base >> 27;
        region_list->super_map = 0xFFFFFFFFFFFFFFFFULL;

        // Flags
        if(base < 0x1000000) region_list->flags.type = RTYPE_LOW;
        else if(base <= 0xFFFFFFFF) region_list->flags.type = RTYPE_32BIT;
        else region_list->flags.type = RTYPE_64BIT;
        region_list->flags.reserved = 0xFFU;
        region_list->flags.present = 1;

        tail = list_get_tail(region_list);
        tail->bitmap = (uint64_t*)(base - 0x1000);

        for(size_t i = 0; i < 512 && tail->bitmap != KNULL; i++)
        {
            tail->bitmap[i] = 0xFFFFFFFFFFFFFFFFULL;
        }

        // Set bits for allocated blocks
        if(base_in_bda) bm_set_bit(region_list, (base - 0x3000) >> 12);
        bm_set_bit(region_list, (base - 0x2000) >> 12);
        bm_set_bit(region_list, (base - 0x1000) >> 12);
    }
    if((base >> 27) > (tail->base >> 27))
    {
        if(type != 1 && type != 3) return;

        // Alloc new block & ptr
        tail = mm_create_region(tail, base >> 27);
    }

    // Set bits according to type
    for(size_t block = 0;;)
    {
        for(size_t bit = 0; bit < ((length >> 12) & 0x7FFF); bit++)
        {
            if(type == 1)
            {
                bm_clear_bit(tail, bit + (base >> 12));
            }
            else bm_set_bit(tail, bit + (base >> 12));
        }

        if(cover_kernel)
        {
            // Reserve kernel image region
            mm_add_region(    (unsigned long)&kernel_phystart,
                            (size_t)&kernel_physize,
                            2);
            cover_kernel = false;
        }

        if(++block >= (length >> 27)) break;

        // Alloc new block & ptr
        tail = mm_create_region(tail, (base >> 27) + block);
    }
}

/*
 * Finds a free memory block with the specified size.
 * Size is in 4KiB blocks.
 * Returns a pointer which matches the criteria, or KNULL if none was found.
 */
unsigned long mm_alloc(size_t size)
{
    size_t bit_index = 0xFFFF;
    mem_region_t* region = list_search_free(region_list);

    if(size >= 32768)
    {
        // TODO: Are allocations larger than 128MiB needed?
        return (unsigned long)KNULL;
    } else
    {
        // Find index
        while(region != KNULL)
        {
            bit_index = bm_find_free_bits(region, size);
            if(bit_index != 0xFFFF) break;
            region = list_search_free(region->next);
        }

        // Check if a block was actually found
        if(region == KNULL && bit_index == 0xFFFF)
        {
            kpanic("Out of Memory");
            return (unsigned long)KNULL;
        }

        // Set bits in bitmap
        for(size_t i = 0; i < size; i++)
            bm_set_bit(region, bit_index + i);

        return (region->base << 27) | (bit_index << 12);
    }
}

/*
 * Frees a memory block allocated by mm_alloc
 */
void mm_free(unsigned long addr, size_t size)
{
    if(addr == (unsigned long)KNULL || size == 0 || size >= 32768) return;

    mem_region_t* region = list_search_block(region_list, (size_t)addr);
    if(region == KNULL)
        return; // None of the mappings contain this address

    size_t frame_ptr = (((size_t)addr) >> 12) & 0x7FFF;

    for(size_t i = 0; i < size; i++)
        bm_clear_bit(region, frame_ptr + i);
}
