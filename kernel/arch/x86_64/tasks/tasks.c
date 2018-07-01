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

extern void initialize_thread();

/****SUPER TEMPORARY ADDRESS ALLOCATION****/
#ifndef __K4_VISUAL_STACK__
static uint64_t alloc_base = 0x00007FFFFFFFF000;
#else
static uint64_t alloc_base = 0xFFFFE00000080000;
#endif

uint64_t alloc_address()
{
	alloc_base -= 0x1000; // 4KiB guard page
	uint64_t retval = alloc_base;
	alloc_base -= 0x4000; // 16KiB store
	return retval;
}

void init_register_state(thread_t *thread, uint64_t *entry_point)
{
	thread->register_state = kmalloc(sizeof(struct thread_registers));
	memset(thread->register_state, 0, sizeof(struct thread_registers));

	uint64_t *kernel_stack = (uint64_t*) alloc_address();
	struct thread_registers *registers = thread->register_state;

	registers->rsp = alloc_address();

	hal_save_interrupts();
	paging_context_t* last_context = mmu_current_context();
	mmu_switch_context(thread->parent->page_context_base);
	for(uint64_t i = 0; i < 4; i++)
	{
		mmu_map((uintptr_t)kernel_stack - (i << 12));
		mmu_map(registers->rsp - (i << 12));
	}

	// IRET structure
	*(kernel_stack--) = 0x00; // SS
	*(kernel_stack--) = registers->kernel_rsp; // RSP
	*(kernel_stack--) = 0x0202; // RFLAGS
	*(kernel_stack--) = 0x08; // CS
	*(kernel_stack--) = (uint64_t) entry_point; // RIP

	// initialize_thread stack
	*(kernel_stack--) = (uint64_t) initialize_thread; // Return address
	*(kernel_stack--) = (uint64_t) thread; // RBP
	kernel_stack -= 4; // R15-12, RBX
	registers->kernel_rsp = (uint64_t)kernel_stack;

	// General registers
	registers->rip = (uint64_t) entry_point;
	mmu_switch_context(last_context);
	hal_restore_interrupts();
}

void cleanup_register_state(thread_t *thread)
{
	struct thread_registers *registers = thread->register_state;

	hal_save_interrupts();
	paging_context_t* last_context = mmu_current_context();
	mmu_switch_context(thread->parent->page_context_base);
	for(uint64_t i = 0; i < 4; i++)
	{
		mmu_unmap((registers->kernel_rsp & ~0xFFF) - (i << 12));
		mmu_unmap((registers->rsp & ~0xFFF) - (i << 12));
	}
	mmu_switch_context(last_context);
	hal_restore_interrupts();
}
