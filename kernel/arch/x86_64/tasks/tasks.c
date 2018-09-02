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

#include <common/tasks.h>
#include <common/hal.h>
#include <common/mm.h>
#include <x86_64/stack_state.h>

extern void __initialize_thread();

/****SUPER TEMPORARY ADDRESS ALLOCATION****/
#ifndef __K4_VISUAL_STACK__
static uint64_t alloc_base = 0x00007FFFFFFFF000;
#else
static uint64_t alloc_base = 0xFFFFE00000080000;
#endif

uint64_t alloc_address()
{
	alloc_base -= 0x4000; // 16KiB store
	uint64_t retval = alloc_base;
	alloc_base -= 0x1000; // 4KiB guard page
	return retval;
}

/**
 * Initializes the architecture-specfic side of a thread
 */
void init_register_state(thread_t *thread, uint64_t *entry_point, unsigned long* kernel_stack)
{
	//thread->register_state.rsp = alloc_address();
	thread->kernel_sp = (unsigned long)kernel_stack;
	thread->kernel_stacktop = (unsigned long)kernel_stack;

	uint64_t* thread_stack = (uint64_t*)thread->kernel_sp;
	// IRET structure
	*(--thread_stack) = 0x010;
	*(--thread_stack) = thread->kernel_sp;
	*(--thread_stack) = 0x202;
	*(--thread_stack) = 0x008;
	*(--thread_stack) = entry_point;

	// Initialize Thread entry
	*(--thread_stack) = __initialize_thread;
	*(--thread_stack) = thread;
	thread_stack -= 5; // Remaining preserved registers

	thread->kernel_sp = (uint64_t)thread_stack;
}

void cleanup_register_state(thread_t *thread)
{
	kfree(thread->kernel_stacktop - THREAD_STACK_SIZE);
}
