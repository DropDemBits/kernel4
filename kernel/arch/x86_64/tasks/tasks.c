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

#include <common/hal.h>
#include <common/mm/mm.h>
#include <common/sched/sched.h>
#include <common/tasks/tasks.h>

extern void __initialize_thread();

/****SUPER TEMPORARY ADDRESS ALLOCATION****/
// #ifndef __K4_VISUAL_STACK__
static uint64_t alloc_base = 0xFFFF900000000000;
// #else
// static uint64_t alloc_base = 0xFFFFE00000080000;
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
        uint64_t stack = alloc_address();
        thread->kernel_sp = stack;
        thread->kernel_stacktop = stack;

        // Map stack pages
        for(int i = 0; i < (THREAD_STACK_SIZE >> 12); i++)
            mmu_map((void*)(thread->kernel_stacktop - (i << 12)), mm_alloc(1), MMU_FLAGS_DEFAULT);
    }

    volatile uint64_t * thread_stack = (uint64_t*)thread->kernel_sp;
    
    // Entry point structure
    *(--thread_stack) = (uint64_t)sched_terminate; // Return to sched_terminate()

    thread->kernel_sp = (uint64_t)thread_stack;

    // IRET structure
    *(--thread_stack) = 0x010;
    *(--thread_stack) = thread->kernel_sp;
    *(--thread_stack) = 0x202;
    *(--thread_stack) = 0x008;
    *(--thread_stack) = (uint64_t)entry_point;

    // Initialize Thread entry
    *(--thread_stack) = (uint64_t)__initialize_thread;
    *(--thread_stack) = (uint64_t)thread;

    // Parameters passed to entry point
    *(--thread_stack) = (uint64_t)params; // RBX
    thread_stack -= 4; // Remaining preserved registers

    thread->kernel_sp = (uint64_t)thread_stack;
}

void cleanup_register_state(thread_t *thread)
{
    
}
