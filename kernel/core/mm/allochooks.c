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

#include <common/kfuncs.h>
#include <common/liballoc.h>
#include <common/hal.h>
#include <common/mm.h>
#include <common/tty.h>
#include <common/sched.h>

static size_t heap_base = 0;
static size_t heap_limit = 0;
static size_t free_base = 0;

static size_t alloc_memblocks(size_t length)
{
    if(free_base + (length << 12) > heap_limit) return 0;
    size_t retval = free_base;
    bool errored = false;

    for(; length > 0; length--)
    {
        if(mmu_map(free_base) != 0)
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
        mmu_unmap(free_base);
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
    // TODO: Disable preemption
    //preempt_disable();
    hal_save_interrupts();
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
    // TODO: Enable preemption
    //preempt_enable();
    hal_restore_interrupts();
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
