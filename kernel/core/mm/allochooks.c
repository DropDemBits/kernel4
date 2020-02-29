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

#include <common/hal.h>
#include <common/util/kfuncs.h>
#include <common/mm/liballoc.h>
#include <common/mm/mm.h>
#include <common/tty/tty.h>

#include <string.h>

#ifdef ENABLE_HEAP_DEBUG
#define HEAP_DBG(...) klog_logln(LVL_DEBUG, __VA_ARGS__)
#else
#define HEAP_DBG(...)
#endif

// Maximum number of entries allowed
#define MAX_ENTRIES 4096
// Number of entries per physical page
#define ENTRIES_PER_PAGE (PAGE_SIZE / sizeof(struct free_memblock))

// One free memblock
struct free_memblock
{
    uintptr_t base; // Start of the free address area
    size_t length;  // Size of the free address area, in pages
};

static uintptr_t heap_base = 0;
static uintptr_t heap_limit = 0;
static uintptr_t alloc_base = 0;
static size_t alloc_count = 0; // Number of pages allocated

// Pointer to the free address manager
// ???: Rebuild into a tree?
static struct free_memblock* free_memblock_region = NULL;
static size_t num_entries = 0;

/**
 * @brief  Allocates the next address block
 * @note   
 * @param  length: The length of the new address block, in pages
 * @retval The pointer to the new address block, or 0 if an error occurred
 */
static uintptr_t get_next_address(size_t length)
{
    uintptr_t alloc_addr;

    // First, check for a space that has the required length
    int entry_candidate = -1;
    struct free_memblock* entry = KNULL;

    for (size_t i = 0; i < num_entries; i++)
    {
        entry = &free_memblock_region[i];
        if (entry->length >= length)
        {
            // Found an entry
            entry_candidate = i;
            break;
        }
    }

    if (entry_candidate != -1)
    {
        // Yoink address space from the beginning of the range
        alloc_addr = entry->base;

        if (entry->length > length)
        {
            size_t last_len = entry->length;
            
            // Shrink the current entry
            entry->base   += (length << PAGE_SHIFT);
            entry->length -= length;

            HEAP_DBG("[Shrink (%p -> %p)]", entry->base, entry->base + (entry->length << PAGE_SHIFT));

            if (entry->length == 0 || entry->length > last_len)
                kpanic("Heap_Free entry %d shrank to size of %d", entry_candidate, (ssize_t)entry->length);
        }
        else
        {
            // Remove the current entry, take the last entry from the list and
            // move it to the old location
            --num_entries;
            struct free_memblock *last_entry = &free_memblock_region[num_entries];

            memcpy(entry, last_entry, sizeof(*last_entry));
            memset(last_entry, 0x00, sizeof(*last_entry));

            HEAP_DBG("[Ent Reloc %d -> %d]", num_entries, entry_candidate);
        }
    }
    else
    {
        // No candidate entry, grab a page from the watermark allocator
        if(alloc_base + (length << PAGE_SHIFT) > heap_limit)
            kpanic("Ran out of heap space!");

        alloc_addr = alloc_base;
        alloc_base += length << PAGE_SHIFT;
        HEAP_DBG("[mark -> %p]", alloc_base);
    }

    alloc_count += length;
    HEAP_DBG("Grow, now start at %p (%u kiB used, mark %u kiB) [Alloc gave %p]",
                 alloc_base,
                (alloc_count << PAGE_SHIFT) >> 10,
                (alloc_base - heap_base) >> 10,
                 alloc_addr);
    return alloc_addr;
}

/**
 * @brief  Puts the given memory area into the free memory area
 * @note   
 * @param  free_base: The starting address of the free area
 * @param  length: The length of the free area, in pages
 * @retval None
 */
static void put_next_address(uintptr_t free_base, size_t length)
{
    alloc_count -= length;
    HEAP_DBG("Shrink, using less pages (%u kiB used, mark %u kiB)",
                (alloc_count << PAGE_SHIFT) >> 10,
                (alloc_base - heap_base) >> 10);

    // Start looking through all of the entries for a free slot
    uintptr_t free_limit = free_base + (length << PAGE_SHIFT);

    int empty_candidate = -1;

    for (size_t i = 0; i < num_entries; i++)
    {
        struct free_memblock* entry = &free_memblock_region[i];
        uintptr_t entry_base = entry->base;
        uintptr_t entry_limit = entry->base + (entry->length << PAGE_SHIFT);

        if (entry->base != 0)
        {
            // Check for overlapping entries
            if (free_base <= entry_limit && free_limit >= entry_base)
            {
                // Entries overlap, merge

                if (free_base > entry_base && entry_limit > free_limit)
                {
                    // Free entry is completely contained in this entry
                    // Shouldn't happen, but not fatal
                    klog_logln(LVL_WARN, "Heap free region is contained in an already larger region (%p -> %p contained in %p -> %p)",
                        free_base, free_limit, entry_base, entry_limit);
                    return;
                }

                // Take the smaller address for the new base
                if (free_base < entry_base)
                    entry_base = free_base;

                // Take the bigger address for the new limit
                if (free_limit > entry_limit)
                    entry_limit = free_limit;

                entry->base = entry_base;
                entry->length = (entry_limit - entry_base) >> PAGE_SHIFT;

                HEAP_DBG("Merging blocks (%p -> %p)",
                        entry_base, entry_base + (entry->length << PAGE_SHIFT));

                // Done!
                return;
            }
        }
        else if (empty_candidate == -1)
        {
            // Add entry to the first empty candidate
            empty_candidate = (int)i;
        }
    }

    // No mergable regions found, allocate a new one
    if (empty_candidate == -1)
    {
        // TODO: If at the maximum number of entries, try and merge big regions

        if (num_entries == MAX_ENTRIES)
            kpanic("Unable to allocate heap free block (Heavily fragmented heap?)");
        
        struct free_memblock* entry = &free_memblock_region[num_entries];
        entry->base = free_base;
        entry->length = length;
        num_entries++;

        // Done!
        return;
    }
}

static uintptr_t alloc_memblocks(size_t length)
{
    uintptr_t alloc_start = get_next_address(length);
    if (!alloc_start)
        return 0;

    HEAP_DBG("Allocating heap memory to address %p (%d pages)", alloc_start, length);

    bool errored = false;

    for(size_t pfo = 0; pfo < length; pfo++)
    {
        void* page = (void*)(alloc_start + (pfo << PAGE_SHIFT));
        size_t phy_address = mm_alloc(1);
        int status = mmu_map(page, phy_address, MMU_FLAGS_DEFAULT);

        if(status != 0)
        {
            errored = true;
            klog_logln(LVL_ERROR, "Error mapping %p: %d", page, status);
            break;
        }
    }

    if(errored)
    {
        // Various places in kernel code don't check for a null pointer, so just panic
        kpanic("Could not allocate heap memory (Out of memory?)");
    }

    return alloc_start;
}

static void free_memblocks(void* base, size_t length)
{
    uintptr_t free_page = (uintptr_t)base;
    HEAP_DBG("Freeing heap memory from address %p (%d pages)", base, length);

    // Free each page
    for(size_t pfo = 0; pfo < length; pfo++)
    {
        void* page = (void*)(free_page + (pfo << PAGE_SHIFT));
        unsigned long phy_addr = mmu_get_mapping(page);
        mmu_unmap(page, true);
        mm_free(phy_addr, 1);
    }

    // Put back each address
    put_next_address((uintptr_t)base, length);
    HEAP_DBG("Addr Given: %p %u pages", base, length);
}

void heap_init()
{
    HEAP_DBG("Initializing kernel heap");
    heap_base = (uintptr_t) get_heap_info()->base;
    heap_limit = (uintptr_t) (get_heap_info()->length + heap_base);
    alloc_base = heap_base;

    // Len in page frames
    size_t free_memblock_len = MAX_ENTRIES / ENTRIES_PER_PAGE;
    free_memblock_region = (struct free_memblock*)(heap_limit - (free_memblock_len << PAGE_SHIFT));

    // Initialize the free address manager by allocating the free entry list
    for (size_t pfo = 0; pfo < free_memblock_len; pfo++)
    {
        void* page = (void*)((uintptr_t)free_memblock_region + (pfo << PAGE_SHIFT));
        size_t phy_addr = mm_alloc(1);

        if (phy_addr == (uintptr_t)KNULL)
            kpanic("Unable to initialize heap free address manager (Out of memory?)");

        int errcode = mmu_map(page, phy_addr, MMU_FLAGS_DEFAULT);
        if (errcode < 0)
            kpanic("Unable to map page %p to %p (%d)", page, phy_addr, errcode);
    }

    // Clear the area
    memset(free_memblock_region, 0, free_memblock_len << PAGE_SHIFT);
}

/** This function is supposed to lock the memory data structures. It
 * could be as simple as disabling interrupts or acquiring a spinlock.
 * It's up to you to decide.
 *
 * \return 0 if the lock was acquired successfully. Anything else is
 * failure.
 */
int liballoc_lock()
{
    // TODO: Do something with spinlocks when doing SMP
    // TODO: !!! REPLACE WITH A MUTEX !!!
    sched_lock();
    preempt_disable();
    return 0;
}

/** This function unlocks what was previously locked by the liballoc_lock
 * function.  If it disabled interrupts, it enables interrupts. If it
 * had acquiried a spinlock, it releases the spinlock. etc.
 *
 * \return 0 if the lock was successfully released.
 */
int liballoc_unlock()
{
    // TODO: Do something with spinlocks when doing SMP
    preempt_enable();
    sched_unlock();
    return 0;
}

/** This is the hook into the local system which allocates pages. It
 * accepts an integer parameter which is the number of pages
 * required.  The page size was set up in the liballoc_init function.
 *
 * \return NULL if the pages were not allocated.
 * \return A pointer to the allocated memory.
 */
void* liballoc_alloc(size_t num_pages)
{
    return (void*)alloc_memblocks(num_pages);
}

/** This frees previously allocated memory. The void* parameter passed
 * to the function is the exact same value returned from a previous
 * liballoc_alloc call.
 *
 * The integer value is the number of pages to free.
 *
 * \return 0 if the memory was successfully freed.
 */
int liballoc_free(void* base, size_t num_pages)
{
    free_memblocks(base, num_pages);
    return 0;
}
