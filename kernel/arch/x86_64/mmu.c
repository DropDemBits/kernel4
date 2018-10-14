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

#include <common/util/kfuncs.h>
#include <common/mm/mm.h>
#include <common/tty/fb.h>
#include <common/tty/tty.h>

#include <arch/idt.h>
#include <stack_state.h>

struct PageError {
    uint32_t was_present : 1;
    uint32_t was_write : 1;
    uint32_t was_user : 1;
    uint32_t was_reserved_set : 1;
    uint32_t was_instruction_fetch : 1;
    uint32_t was_pk_violation : 1;
};

struct PageEntry {
    uint64_t p : 1;   // Present bit
    uint64_t rw : 1;  // Read/Write bit
    uint64_t su : 1;  // Supervisor/User bit
    uint64_t pwt : 1; // Page Write-through
    uint64_t pcd : 1; // Page cache disable
    uint64_t a : 1;   // Accessed bit
    uint64_t d : 1;   // Dirty bit
    uint64_t pat : 1; // Present bit
    uint64_t g : 1;   // Global
    uint64_t avl : 3; // Available for use
    uint64_t frame : 36;

    uint64_t rsv : 4; // Reserved bits
    uint64_t ign : 7; // Ignored (Available) bits
    uint64_t pk : 4;  // Protection Key
    uint64_t xd : 1;  // NX bit
} __attribute__((__packed__));
typedef struct PageEntry page_entry_t;

struct PageStruct {
    struct PageEntry page_entry[512];
} __attribute__((__packed__));
typedef struct PageStruct page_struct_t;

enum {
    PML4_SHIFT = 12+(9*3),
    PDPT_SHIFT = 12+(9*2),
    PDT_SHIFT  = 12+(9*1),
    PTT_SHIFT  = 12+(9*0),
    PML4E_MASK = 0x0000001FF,
    PDPE_MASK  = 0x00003FFFF,
    PDE_MASK   = 0x007FFFFFF,
    PTE_MASK   = 0xFFFFFFFFF,
};

static uint64_t temp_map_base =                             0xFFFFFF0000000000;
static bool using_temp_map = false;

static paging_context_t* current_context;
static paging_context_t* temp_context;
static paging_context_t initial_context;

static uint64_t const temp_remap_offset =                    0x0000008000000000;
static page_entry_t* const pml4e_lookup = (page_entry_t*)    0xFFFFFFFFFFFFF000;
static page_entry_t* const pdpe_lookup  = (page_entry_t*)    0xFFFFFFFFFFE00000;
static page_entry_t* const pde_lookup   = (page_entry_t*)    0xFFFFFFFFC0000000;
static page_entry_t* const pte_lookup   = (page_entry_t*)    0xFFFFFF8000000000;

static void invlpg(unsigned long address)
{
    asm volatile("invlpg (%%rax)" :: "a"(address));
}

static page_entry_t* get_lookup(page_entry_t* base)
{
    if(using_temp_map) return (page_entry_t*)((uintptr_t)base - temp_remap_offset);
    else return base;
}

static page_entry_t* get_pml4e_entry(unsigned long address)
{
    uint64_t pml4e_off = (address >> PML4_SHIFT) & PML4E_MASK;

    return &(get_lookup(pml4e_lookup)[pml4e_off]);
}

static page_entry_t* get_pdpe_entry(unsigned long address)
{
    uint64_t pdpe_off = ((uintptr_t)address >> PDPT_SHIFT) & PDPE_MASK;

    return &(get_lookup(pdpe_lookup)[pdpe_off]);
}

static page_entry_t* get_pde_entry(unsigned long address)
{
    uint64_t pde_off = ((uintptr_t)address >> PDT_SHIFT) & PDE_MASK;

    return &(get_lookup(pde_lookup)[pde_off]);
}

static page_entry_t* get_pte_entry(unsigned long address)
{
    uint64_t pte_off = ((uintptr_t)address >> PTT_SHIFT) & PTE_MASK;

    return &(get_lookup(pte_lookup)[pte_off]);
}

// Returns true if a new structure was allocated, false otherwise.
static bool check_and_map_entry(page_entry_t* entry, unsigned long address)
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
            unsigned long frame = mm_alloc(1);
            if(frame == (unsigned long)KNULL)
            {
                entry->frame = KMEM_POISON;
                return false;
            }

            entry->frame = frame >> 12;
            entry->p = 1;
            entry->rw = 1;
            entry->xd = 0;
            entry->rsv = 0;

            if((uintptr_t)address < 0xFFFF800000000000)
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
    struct PageError *page_error = (struct PageError*)&frame->err_code;
    uint64_t address = frame->cr2;

    if(address < 0x400000)
    {
        if(page_error->was_instruction_fetch)
        {
            kpanic_intr(frame, "Jump to null of offset %#p", address);
        } else
        {
            kpanic_intr(frame, "Null pointer of offset %#p", address);
        }
    } else
    {
        kpanic_intr(frame, "Page fault at %#p (error code %x)", address, frame->err_code);
    }
}

void mmu_init()
{
    // Kill identity mapping
    uint64_t* ptr = (uint64_t*)0xFFFFFFFFFFFFF000;
    *ptr = 0;

    isr_add_handler(14, (isr_t)pf_handler, NULL);
    
    // Setup initial paging context
    uint64_t cr3;
    asm volatile("movq %%cr3, %%rax\n\t"
                 "movq %%rax, %%cr3\n\t":
        "=a"(cr3));

    initial_context.phybase = cr3;
    current_context = &initial_context;
}

int mmu_map_direct(unsigned long address, unsigned long mapping)
{
    if(mapping == (uintptr_t)KNULL || address == (uintptr_t)KNULL) return -1;

    // TODO: Add PML5 support
    // Check PML4E Presence
    if(check_and_map_entry(get_pml4e_entry(address), address))
    {
        uint64_t pdpe_base = ((uintptr_t)address >> PDPT_SHIFT) & (PDPE_MASK & ~0x1FF);
        memset(&(get_lookup(pdpe_lookup)[pdpe_base]), 0x00, 0x1000);
    }
    else if(get_pml4e_entry(address)->frame == KMEM_POISON || !get_pml4e_entry(address)->frame)
        return -1;

    // Check PDPE Presence
    if(check_and_map_entry(get_pdpe_entry(address), address))
    {
        uint64_t pde_base = ((uintptr_t)address >> PDT_SHIFT) & (PDE_MASK & ~0x1FF);
        memset(&(get_lookup(pde_lookup)[pde_base]), 0x00, 0x1000);
    }
    else if(get_pdpe_entry(address)->frame == KMEM_POISON || !get_pml4e_entry(address)->frame)
        return -1;

    // Check PDE Presence
    if(check_and_map_entry(get_pde_entry(address), address))
    {
        uint64_t pte_base = ((uintptr_t)address >> PTT_SHIFT) & (PTE_MASK & ~0x1FF);
        memset(&(get_lookup(pte_lookup)[pte_base]), 0x00, 0x1000);
    }
    else if(get_pde_entry(address)->frame == KMEM_POISON || !get_pml4e_entry(address)->frame)
        return -1;

    // Check if we are trying to map to an already mapped address
    if(get_pte_entry(address)->p)
        return -2;

    // This is an unmapped address, so map it
    get_pte_entry(address)->xd = 0;
    get_pte_entry(address)->rsv = 0;
    get_pte_entry(address)->p = 1;
    get_pte_entry(address)->rw = 1;
    get_pte_entry(address)->frame = mapping >> 12;

    if((uintptr_t)address < 0xFFFF800000000000)
        get_pte_entry(address)->su = 1;
    else
        get_pte_entry(address)->su = 0;

    invlpg(address);
    return 0;
}

int mmu_map(unsigned long address)
{
    unsigned long frame = mm_alloc(1);

    if(frame == (uintptr_t)KNULL)
        return -1;

    int retval = mmu_map_direct(address, frame);
    if(retval == -1LL) kpanic("OOPSIES");
    if(retval == -2LL) kpanic("HELP MEH");
    return retval;
}

static void mmu_unmap_direct(unsigned long address)
{
    get_pte_entry(address)->p = 0;
    get_pte_entry(address)->rw = 0;
    get_pte_entry(address)->su = 0;
    invlpg(address);
}

void mmu_unmap(unsigned long address)
{
    if(    get_pml4e_entry(address)->p == 0 ||
        get_pdpe_entry(address)->p == 0 ||
        get_pde_entry(address)->p == 0 ||
        get_pte_entry(address)->p == 0) return;

    mmu_unmap_direct(address);
    mm_free(get_pte_entry(address)->frame << 12, 1);
    get_pte_entry(address)->frame = KMEM_POISON;
}

bool mmu_is_usable(unsigned long address)
{
    if(    get_pml4e_entry(address)->p &&
        get_pdpe_entry(address)->p &&
        get_pde_entry(address)->p &&
        get_pte_entry(address)->p) return true;
    return false;
}

void* mm_get_base()
{
    return (void*) 0xFFFF880000000000;
}

paging_context_t* mmu_create_context()
{
    // Create page context base
    unsigned long pml4_context = mm_alloc(1);

    // Map context to temporary address
    mmu_map_direct(temp_map_base, pml4_context);
    memset((void*)temp_map_base, 0x00, 0x1000);

    // Copy relavent mappings to address space (Excluding temporary and recursive mapping)
    memcpy((uint8_t*)temp_map_base+2048, (uint8_t*)pml4e_lookup+2048, 2048-16);

    // Change recursive mapping entry
    page_entry_t *pml4e_temp_lookup = (page_entry_t*)temp_map_base;
    pml4e_temp_lookup[511].frame = pml4_context >> 12ULL;
    pml4e_temp_lookup[511].xd = 0;
    pml4e_temp_lookup[511].rw = 1;
    pml4e_temp_lookup[511].p = 1;

    paging_context_t *context = kmalloc(sizeof(paging_context_t));
    context->phybase = pml4_context;

    mmu_unmap_direct(temp_map_base);
    return context;
}

void mmu_destroy_context(paging_context_t* context)
{
    if(context == KNULL) return;
    mm_free(context->phybase, 1);
    kfree(context);
}

paging_context_t* mmu_current_context()
{
    return current_context;
}

void mmu_set_context(paging_context_t* addr_context)
{
    current_context = addr_context;
}

void mmu_set_temp_context(paging_context_t* addr_context)
{
    if(current_context == addr_context)
    {
        // Don't bother setting the temp context, as we are just using the same context
        using_temp_map = false;
        return;
    }

    if(temp_context == addr_context) goto start_temp; // No point in overwritting it.
    temp_context = addr_context;
    
    // Overwrite the temporary mapping entry
    pml4e_lookup[510].frame = temp_context->phybase >> 12ULL;
    invlpg((uintptr_t)&(pml4e_lookup[510]));
    
start_temp:
    using_temp_map = true;
}

void mmu_exit_temp_context()
{
    using_temp_map = false;
}

void mmu_switch_context(paging_context_t* addr_context)
{
    // If we're switching to the same address context, then don't bother
    if(addr_context == current_context) return;
    current_context = addr_context;
    asm volatile("movq %%rax, %%cr3\n\t"::
        "a"(addr_context->phybase));
}
