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

#include <common/mm.h>
#include <common/tty.h>
#include <common/kfuncs.h>
#include <i386/stack_state.h>
#include <i386/idt.h>

struct PageError {
	uint32_t was_present : 1;
	uint32_t was_write : 1;
	uint32_t was_user : 1;
	uint32_t was_reserved_set : 1;
	uint32_t was_instruction_fetch : 1;
	uint32_t was_pk_violation : 1;
};

struct PageEntry {
	uint32_t p : 1;   // Present bit
	uint32_t rw : 1;  // Read/Write bit
	uint32_t su : 1;  // Supervisor/User bit
	uint32_t pwt : 1; // Page Write-through
	uint32_t pcd : 1; // Page cache disable
	uint32_t a : 1;   // Accessed bit
	uint32_t d : 1;   // Dirty bit
	uint32_t pat : 1; // Present bit
	uint32_t g : 1;   // Global
	uint32_t avl : 3; // Available for use
	uint32_t frame : 20;
} __attribute__((__packed__));
typedef struct PageEntry page_entry_t;

struct PageStruct {
	page_entry_t page_entry[1024];
};
typedef struct PageStruct page_struct_t;

enum {
	PDT_SHIFT = 12+(10*1),
	PTT_SHIFT = 12+(10*0),
	PDE_MASK  = 0x003FF,
	PTE_MASK  = 0xFFFFF,
};

static paging_context_t initial_context;
static paging_context_t* current_context;
static page_entry_t* const temp_mapping = (page_entry_t*) 0xF0000000;
static page_entry_t* const pde_lookup = (page_entry_t*) 0xFFFFF000;
static page_entry_t* const pte_lookup = (page_entry_t*) 0xFFC00000;

static void invlpg(linear_addr_t* address)
{
	asm volatile("invlpg (%%eax)" : "=a"(address));
}

static page_entry_t* get_pde_entry(linear_addr_t* address)
{
	uint32_t pde_off = ((linear_addr_t)address >> PDT_SHIFT) & PDE_MASK;

	return &(pde_lookup[pde_off]);
}

static page_entry_t* get_pte_entry(linear_addr_t* address)
{
	uint32_t pte_off = ((linear_addr_t)address >> PTT_SHIFT) & PTE_MASK;

	return &(pte_lookup[pte_off]);
}

// Returns true if a new structure was allocated, false otherwise.
static bool check_and_map_entry(page_entry_t* entry, linear_addr_t* address)
{
	if(!entry->p)
	{
		bool new_page_entry = true;

		if(entry->frame != (uintptr_t)KNULL && entry->frame != 0)
		{
			// Page entry is simply not present, but has a valid address
			entry->p = 1;
			new_page_entry = false;
		} else
		{
			// Allocate a new entry
			uintptr_t frame = (uintptr_t)mm_alloc(1);
			if(frame == (uintptr_t)KNULL)
				return -1;

			entry->frame = frame >> 12;
			entry->p = 1;
			entry->rw = 1;

			if((size_t)address < 0x80000000)
				entry->su = 1;
			else
				entry->su = 0;
		}

		invlpg(address);
		return new_page_entry;
	}

	return false;
}

void pf_handler(struct intr_stack *frame)
{
	struct PageError *page_error = (struct PageError*)frame->err_code;
	uint32_t address = frame->cr2;

	if(frame->cr2 >= 0xE0000000 && frame->cr2 < 0xF0000000)
	{
		// Our visual output devices are bork'd, so fall back onto serial.
		tty_add_output(FB_CONSOLE, (size_t)KNULL);
		tty_add_output(VGA_CONSOLE, (size_t)KNULL);
	}

	kpanic_intr(frame, "Page fault at %#p (error code %x)", address, page_error);
	// This shouldn't return
}

void mmu_init()
{
	// Set CR3
	uint32_t cr3 = 0;
	asm volatile("movl %%cr3, %%eax\n\t"
		: "=a"(cr3));
	
	initial_context.phybase = (uint64_t)cr3;
	current_context = &initial_context;
	
	// Get rid of Identity Mapping
	for(int i = 0; i < 0x8; i++)
	{
		void* addr = (void*)(i << PDT_SHIFT);
		if(!get_pde_entry(addr)->p) continue;

		get_pde_entry(addr)->frame = 0;
		get_pde_entry(addr)->p = 0;
		invlpg(addr);
	}

	isr_add_handler(14, (isr_t)pf_handler);
}

int mmu_map_direct(linear_addr_t* address, physical_addr_t* mapping)
{
	if(mapping == KNULL || address == KNULL) return -1;

	// Check PDE Presence
	if(check_and_map_entry(get_pde_entry(address), address))
	{
		uint64_t pte_base = ((uintptr_t)address >> PTT_SHIFT) & (PTE_MASK & ~0x3FF);
		memset(&(pte_lookup[pte_base]), 0x00, 0x1000);
	}
	else if(get_pde_entry(address)->frame == KMEM_POISON)
		return -1;

	// Check if we are trying to map to an already mapped address
	if(get_pte_entry(address)->p)
		return -1;

	// This is an unmapped address, so map it
	get_pte_entry(address)->p = 1;
	get_pte_entry(address)->rw = 1;
	get_pte_entry(address)->frame = ((physical_addr_t)mapping >> 12);

	if((uintptr_t)address < 0x80000000)
		get_pte_entry(address)->su = 1;
	else
		get_pte_entry(address)->su = 0;

	invlpg(address);
	return 0;
}

int mmu_map(linear_addr_t* address)
{
	physical_addr_t* frame = mm_alloc(1);

	if(frame == KNULL)
		return -1;

	return mmu_map_direct(address, frame);
}

static bool mmu_unmap_direct(linear_addr_t* address)
{
	if(	get_pde_entry(address)->p == 0 ||
		get_pte_entry(address)->p == 0) return false;

	get_pte_entry(address)->p = 0;
	invlpg(address);
	return true;
}

void mmu_unmap(linear_addr_t* address)
{
	if(!mmu_unmap_direct(address)) return;

	mm_free((physical_addr_t*)(get_pte_entry(address)->frame << 12), 1);
	get_pte_entry(address)->frame = KMEM_POISON;
}

bool mmu_is_usable(linear_addr_t* address)
{
	if(	get_pde_entry(address)->p &&
		get_pte_entry(address)->p) return true;
	return false;
}

linear_addr_t* mm_get_base()
{
	return (linear_addr_t*) 0x88000000;
}

paging_context_t* mmu_create_context()
{
	uint32_t pdt_context = (uint32_t) mm_alloc(1);
	// Temporarily map it
	mmu_map_direct((linear_addr_t*)temp_mapping, (void*)pdt_context);
	memset((void*)temp_mapping, 0x00, 0x1000);

	// Copy relavent mappings to address space
	memcpy((uint8_t*)temp_mapping+2048, (uint8_t*)pde_lookup+2048, 2048);
	// Change recursive mapping entry
	((page_entry_t*)temp_mapping)[511].frame = pdt_context >> 12;

	paging_context_t *context = kmalloc(sizeof(paging_context_t));
	context->phybase = pdt_context;
	mmu_unmap_direct((linear_addr_t*)temp_mapping);

	return context;
}

void mmu_destroy_context(paging_context_t* context)
{
	if(context == KNULL) return;
	mm_free((uint32_t*)(context->phybase), 1);
	kfree(context);
}

paging_context_t* mmu_current_context()
{
	return current_context;
}

void mmu_switch_context(paging_context_t* addr_context)
{
	// If we're switching to the same address context, then don't bother
	if(addr_context == current_context) return;
	current_context = addr_context;
	uint32_t temp = (uint32_t)addr_context->phybase;
	asm volatile("movl %%eax, %%cr3\n\t"::
		"a"(temp) );
}
