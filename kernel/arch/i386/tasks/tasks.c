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

#include <common/sched/sched.h>
#include <common/tasks/tasks.h>
#include <common/mm/mm.h>

extern void __initialize_thread();
extern void bounce_entry();

/****SUPER TEMPORARY ADDRESS ALLOCATION****/
// #ifndef __K4_VISUAL_STACK__
static uint32_t alloc_base = 0x88000000;
// #else
// static uint32_t alloc_base = 0xE0080000;
// #endif

uint64_t alloc_address()
{
    alloc_base -= 0x1000; // 4KiB guard page
    uint64_t retval = alloc_base;
    alloc_base -= THREAD_STACK_SIZE;
    return retval;
}

/**
 * Initializes the architecture-specfic side of a thread
 */
void init_register_state(thread_t *thread, uint64_t *entry_point, unsigned long* kernel_stack, void* params)
{
    if(kernel_stack != NULL)
    {
        thread->kernel_sp = (unsigned long)kernel_stack;
        thread->kernel_stacktop = (unsigned long)kernel_stack;
    }
    else
    {
        // Allocate a new stack
        uint32_t stack = alloc_address();
        thread->kernel_sp = stack;
        thread->kernel_stacktop = stack;

        // Map stack pages
        for(int i = 0; i < (THREAD_STACK_SIZE >> 12); i++)
            mmu_map_direct(thread->kernel_stacktop - (i << 12), mm_alloc(1));
    }

    uint32_t* thread_stack = (uint32_t*)thread->kernel_sp;

    // Entry point structure
    *(--thread_stack) = params;
    *(--thread_stack) = (uint32_t)sched_terminate; // Return to sched_terminate()
    *(--thread_stack) = (uint32_t)entry_point;

    // IRET structure
    *(--thread_stack) = 0x010;
    *(--thread_stack) = thread->kernel_sp;
    *(--thread_stack) = 0x202;
    *(--thread_stack) = 0x008;
    *(--thread_stack) = (uint32_t)bounce_entry;

    // Initialize Thread entry
    *(--thread_stack) = (uint32_t)__initialize_thread;
    *(--thread_stack) = (uint32_t)thread;
    thread_stack -= 3;

    thread->kernel_sp = (uint32_t)thread_stack;
}

void cleanup_register_state(thread_t *thread)
{
    
}
