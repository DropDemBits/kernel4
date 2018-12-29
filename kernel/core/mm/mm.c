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

#include <string.h>

#include <common/mm/mm.h>
#include <common/util/kfuncs.h>

#define BASE_SHIFT  12      // 4KiB
#define BLOCK_SHIFT 27      // 128MiB
#define LAZY_BITMAP (0xFA1E000000000000ULL)

extern uintptr_t kernel_phystart;
extern size_t kernel_physize;
extern uintptr_t kernel_phypage_end;
extern uint32_t initrd_start;
extern uint32_t initrd_size;
extern void* mm_get_base();

enum {
    TYPE_IGNORE = 0,    // Not a real type, just used to ignore overlapped regions
    TYPE_AVAILABLE,
    TYPE_RESERVED,
    TYPE_ACPI_RECLAIMABLE,
    TYPE_NVS,
    TYPE_BADRAM,
};

struct mem_area
{
    uint64_t base;
    uint64_t length;
    uint32_t type;
};

enum {
    RTYPE_LOW,
    RTYPE_32BIT,
    RTYPE_64BIT,
};

struct mem_flags
{
    uint64_t present : 1;
    uint64_t type : 2;
    uint64_t lazy_bitmap : 1;   // If the bitmap is lazily allocated
    uint64_t reserved : 8;
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

static uint8_t __attribute__((aligned (4096))) init_region_list[4096];
static uint8_t __attribute__((aligned (4096))) init_region_bitmap[4096];

static struct mem_area area_list[64];
static unsigned int next_free_area = 0;

static mem_region_t* region_list = &init_region_bitmap;
static size_t frame_blocks = 0;

static void* mm_base_ptr = NULL;

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
        if(addr >> BLOCK_SHIFT == block->base) break;
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

    if(mem_block->flags.lazy_bitmap)
    {
        // TODO: Canabalize the mm_free zero page & use it to allocate another page
        kpanic("GORP");
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

    // We shouldn't be able to free from lazily allocated pages...
    if(mem_block->flags.lazy_bitmap)
        kpanic("Bad mm_free address (%p, %x)", mem_block, index);

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

    if(mem_block->flags.lazy_bitmap)
        // Lazily allocated pages are free pages
        return 0;

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

static void* get_next_address()
{
    void * addr = mm_base_ptr;
    mm_base_ptr += (uintptr_t)((void*)4096);
    return addr;
}

/*
 * Creates a region block and appends it to list_tail.
 * Base to be passed should be base >> 27 (size of region coverage).
 */
static mem_region_t* mm_create_region(mem_region_t* list_tail,
                                      uint64_t base)
{
    mem_region_t* append = KNULL;
    if(++frame_blocks >= 128)
    {
        // Map a new pointer list
        append = get_next_address();

        if(frame_blocks % 128 == 0)
            mmu_map(append, mm_alloc(1), MMU_FLAGS_DEFAULT);
        frame_blocks = 0;
    }
    else
    {
        // Keep on using the current structure
        append = (mem_region_t*)((size_t)list_tail + sizeof(mem_region_t));
    }

    append->next = KNULL;
    append->base = base;
    append->super_map = ~0ULL;

    if(base == 0x0)
        append->flags.type = RTYPE_LOW;
    else if(base <= 32)
        append->flags.type = RTYPE_32BIT;
    else
        append->flags.type = RTYPE_64BIT;

    append->flags.reserved = 0x0;
    append->flags.present = 0;
    append->flags.lazy_bitmap = 1;
    append->bitmap = LAZY_BITMAP;

    append->flags.present = 1;
    mm_append_block(list_tail, append);
    return append;
}

/**
 * @brief  Finds memory area containg the address
 * @note   
 * @param  address: The address to find the containing area
 * @retval The containing area, or NULL if none was found
 */
static struct mem_area* find_area(unsigned long address)
{
    for(size_t i = 0; i < next_free_area; i++)
    {
        struct mem_area* check = &(area_list[i]);

        if(address >= check->base && address < (check->base + check->length))
            return check;
    }

    return NULL;
}

void mm_early_init()
{
    // Reserve appropriate regions
    // Null page
    mm_add_area(0, 0x1000, TYPE_RESERVED);

    // Kernel region
    mm_add_area((uintptr_t)&kernel_phystart, (size_t)&kernel_physize, TYPE_RESERVED);

    // Initrd area
    mm_add_area(initrd_start, initrd_size, TYPE_RESERVED);

    // Setup the bitmap
    mem_region_t* node = &init_region_list;
    region_list = node;

    memset(init_region_list, 0, sizeof(init_region_list));
    memset(init_region_bitmap, 0xFF, sizeof(init_region_bitmap));

    node->next = KNULL;
    node->base = 0;
    node->super_map = 0xFFFFFFFFFFFFFFFFULL;
    node->flags.type = RTYPE_LOW;
    node->flags.reserved = 0x00U;
    node->flags.present = 1;
    node->flags.lazy_bitmap = 0;
    node->bitmap = (uint64_t*)&init_region_bitmap;

    mm_base_ptr = mm_get_base();

    klog_early_logln(INFO, "Complete list:");
    for(size_t i = 0; i < next_free_area; i++)
    {
        struct mem_area* area = &(area_list[i]);

        if(!area->type)
            continue;
        klog_early_logln(INFO, "    %08.8p | %08.8p | %01d", area->base, area->length, area->type);
        mm_add_region(area->base, area->length, area->type);
    }
}

void mm_init()
{
    heap_init();
}

static void add_area(unsigned long base, unsigned long length, uint32_t type)
{
    if(next_free_area >= 64)
        return;

    struct mem_area* area = &(area_list[next_free_area++]);
    area->base = base;
    area->length = length;
    area->type = type;

    klog_early_logln(INFO, "      : %08.8p | %08.8p | %01d", base, length, type);
}

void mm_add_area(unsigned long base, unsigned long length, uint32_t type)
{
    klog_early_logln(INFO, "Create: %08.8p | %08.8p | %01d", base, length, type);

    // Cut out overlapping regiojn
    struct mem_area* base_overlap = find_area(base);
    struct mem_area* limit_overlap = find_area(base + length);

    if(limit_overlap != NULL)
    {
        // A new region will have to be created to deal with the overlap

        // New base at the end of the current region
        uint64_t new_base = base + length;
        uint64_t new_len = limit_overlap->length - (new_base - limit_overlap->base);

        // Add the area and clone the type
        add_area(new_base, new_len, limit_overlap->type);

        // Change the limit_overlap length to the appropriate length
        limit_overlap->length = (new_base - limit_overlap->base);

        if(limit_overlap->length == 0)
            limit_overlap->type = 0;
    }

    if(base_overlap != NULL)
    {
        // Adjust the base side length accordingly
        base_overlap->length = base - base_overlap->base;

        if(base_overlap->length == 0)
            base_overlap->type = 0;
    }

    // Add the actual region last (prevent over-overlapping)
    add_area(base, length, type);
}

// Base address & length in bytes
void mm_add_region(unsigned long base, size_t length, uint32_t type)
{
    mem_region_t *node = list_search_block(region_list, base);

    // Round up length
    length = ((base & 0xFFF) + length + 0xFFF) & ~0xFFF;

    // Check if new block is outside of the last block
    if(node == KNULL || (base >> 27) > (node->base >> 27))
    {
        // Don't make a new block entry for unreclaimable reserved pages
        if(type != MEM_REGION_AVAILABLE && type != MEM_REGION_RECLAIMABLE)
            return;

        // Alloc new block & ptr (append to tail)
        mem_region_t *tail = list_get_tail(region_list);
        node = mm_create_region(tail, base >> 27);
    }

    // Set bits according to type
    unsigned long page_base = (base >> BASE_SHIFT) & 0x7FFF;
    unsigned long block_length = length;

    for(size_t block = 0;;)
    {
        if(block_length < 0x8000000)
        {
            // If less than a full block size, set bits
            size_t num_pages = (length >> BASE_SHIFT) % 0x8000;

            // Exactly 8MiB? Set entire page
            if(!num_pages)
                num_pages = 0x8000;

            if(node->flags.lazy_bitmap)
            {
                // Allocate a new bitmap
                node->flags.lazy_bitmap = 0;
                node->bitmap = get_next_address();
                mmu_map(node->bitmap, mm_alloc(1), MMU_FLAGS_DEFAULT);
                memset(node->bitmap, 0xFF, 4096);
            }

            if(type == MEM_REGION_AVAILABLE)
            {
                for(size_t bit = 0; bit < num_pages; bit++)
                    bm_clear_bit(node, bit + page_base);
            }
        }

        if(++block >= (length >> BLOCK_SHIFT))
            break;

        // Alloc new block & ptr
        mem_region_t *tail = list_get_tail(region_list);
        node = mm_create_region(tail, (base >> BLOCK_SHIFT) + block);

        // Starting from a new block, so use no page offset
        page_base = 0;
        block_length -= 0x8000000;
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
