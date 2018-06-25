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

#include <common/tasks.h>
#include <common/mm.h>
#include <i386/stack_state.h>

extern void initialize_thread();

/****SUPER TEMPORARY ADDRESS ALLOCATION****/
#ifndef __K4_VISUAL_STACK__
static uint32_t alloc_base = 0x7FFFF000;
#else
static uint32_t alloc_base = 0xE0080000;
#endif

uint32_t alloc_address()
{
	alloc_base -= 0x1000; // 4KiB guard page
	uint32_t retval = alloc_base;
	alloc_base -= 0x4000; // 16KiB store
	return retval;
}

void init_register_state(thread_t *thread, uint64_t *entry_point)
{
	thread->register_state = kmalloc(sizeof(struct thread_registers));
	memset(thread->register_state, 0, sizeof(struct thread_registers));
	
	uint32_t *kernel_stack = (uint32_t*) alloc_address();
	struct thread_registers *registers = thread->register_state;

	registers->esp = alloc_address();

	for(uint32_t i = 0; i < 4; i++)
	{
		mmu_map(kernel_stack - (i << 9));
		mmu_map((uint32_t*)(registers->esp - (i << 12)));
	}

	// IRET structure
	registers->kernel_esp = kernel_stack;
	*(kernel_stack--) = 0x10; // SS
	*(kernel_stack--) = registers->kernel_esp; // ESP
	*(kernel_stack--) = 0x0202; // EFLAGS
	*(kernel_stack--) = 0x08; // CS
	*(kernel_stack--) = (uint32_t) entry_point; // EIP

	// initialize_thread stack
	*(kernel_stack--) = (uint32_t) initialize_thread; // Return address
	*(kernel_stack--) = (uint32_t) thread; // EBP
	kernel_stack -= 2; // EDI, ESI, EBX
	registers->kernel_esp = kernel_stack;

	// General registers
	registers->eip = (uint32_t) entry_point;
}

void cleanup_register_state(thread_t *thread)
{
	struct thread_registers *registers = thread->register_state;
	
	for(uint32_t i = 0; i < 4; i++)
	{
		mmu_unmap((uint32_t*)((registers->kernel_esp & ~0xFFF) - (i << 12)));
		mmu_unmap((uint32_t*)((registers->esp & ~0xFFF) - (i << 12)));
	}
}
