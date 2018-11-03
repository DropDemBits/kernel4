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

static size_t heap_base = 0;
static size_t heap_limit = 0;
static size_t free_base = 0;
static uint64_t flags = 0;

static size_t alloc_memblocks(size_t length)
{
    if(free_base + (length << 12) > heap_limit) return 0;
    size_t retval = free_base;
    bool errored = false;

    for(; length > 0; length--)
    {
        if(mmu_map(free_base, mm_alloc(1), MMU_FLAGS_DEFAULT) != 0)
        {
            errored = true;
            break;
        }
        free_base += 0x1000;
    }

    if(errored)
    {
        // Various places don't check for a null pointer, so just panic
        kpanic("Could not allocate heap memory (Out of memory?)");
    }

    return retval;
}

static void free_memblocks(size_t length)
{
    for(; length > 0 && free_base - (length << 12) > heap_base; length--)
    {
        mm_free(mmu_get_mapping(free_base), 1);
        mmu_unmap(free_base, true);
        free_base -= 0x1000;
    }
}

void heap_init()
{
    heap_base = (uint64_t) get_heap_info()->base;
    heap_limit = (uint64_t) (get_heap_info()->length + heap_base);
    free_base = heap_base;
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
    //preempt_disable();
    flags = hal_disable_interrupts();
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
    //preempt_enable();
    hal_enable_interrupts(flags);
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
    free_memblocks(num_pages);
    return 0;
}
