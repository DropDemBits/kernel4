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

#include <common/mm/liballoc.h>
#include <common/mm/mm.h>
#include <common/tty/tty.h>
#include <common/util/kfuncs.h>

#include <arch/iobase.h>
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
    uint32_t p      : 1;    // Present bit
    uint32_t rw     : 1;    // Read/Write bit
    uint32_t su     : 1;    // Supervisor/User bit
    uint32_t pwt    : 1;    // Page Write-through
    uint32_t pcd    : 1;    // Page cache disable
    uint32_t a      : 1;    // Accessed bit
    uint32_t d      : 1;    // Dirty bit
    uint32_t pat    : 1;    // Page Attribute bit
    uint32_t g      : 1;    // Global bit
    uint32_t avl    : 3;    // Available for use
    uint32_t frame  : 20;
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

static uint32_t temp_map_base         =                 0xFF800000;
static bool using_temp_map = false;

static paging_context_t* current_context;
static paging_context_t* temp_context;
static paging_context_t initial_context;

static uint32_t temp_remap_offset     =                 0x00400000;
static page_entry_t* const pde_lookup = (page_entry_t*) 0xFFFFF000;
static page_entry_t* const pte_lookup = (page_entry_t*) 0xFFC00000;

static void invlpg(void* address)
{
    asm volatile("invlpg (%%eax)" : "=a"(address));
}

static page_entry_t* get_lookup(page_entry_t* base)
{
    if(using_temp_map) return (page_entry_t*)((uintptr_t)base - temp_remap_offset);
    else return base;
}

static page_entry_t* get_pde_entry(void* address)
{
    uint32_t pde_off = ((uintptr_t)address >> PDT_SHIFT) & PDE_MASK;

    return &(get_lookup(pde_lookup)[pde_off]);
}

static page_entry_t* get_pte_entry(void* address)
{
    uint32_t pte_off = ((uintptr_t)address >> PTT_SHIFT) & PTE_MASK;

    return &(get_lookup(pte_lookup)[pte_off]);
}

// Returns true if a new structure was allocated, false otherwise.
static bool check_and_map_entry(page_entry_t* entry, void* address, uint32_t flags)
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
                return -1;

            entry->frame = frame >> 12;
            entry->p = 1;
            entry->rw = (flags & MMU_ACCESS_W) >> 1;
            entry->su = (flags & MMU_ACCESS_USER) >> 3;
        }

        invlpg(address);
        return new_page_entry;
    }

    return false;
}

void pf_handler(struct intr_stack *frame, void* args)
{
    struct PageError *page_error = (struct PageError*)&(frame->err_code);
    void* address = (void*)frame->cr2;

    page_entry_t dummy_entry = {};
    memset(&dummy_entry, 0, sizeof(page_entry_t));

    page_entry_t* entry = get_pte_entry(address);
    if(!get_pde_entry(address)->p)
        entry = &dummy_entry;

    kpanic_intr(frame, "Page fault at %#p (error code PWU: %d%d%d, PG PWU: %d%d%d)", address,
                page_error->was_present, page_error->was_write, page_error->was_user,
                entry->p, entry->rw, entry->su);
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
        ((uint32_t*)pde_lookup)[i] = 0;
        invlpg((void*)(i << PDT_SHIFT));
    }

    isr_add_handler(14, pf_handler, NULL);
}

int mmu_map(void* address, unsigned long mapping, uint32_t flags)
{
    int status = 0;

    if(mapping == (unsigned long)KNULL || address == KNULL)
        return MMU_MAPPING_INVAL;

    // Check PDE Presence
    if(check_and_map_entry(get_pde_entry(address), address, flags | MMU_ACCESS_W))
    {
        uint64_t pte_base = ((uintptr_t)address >> PTT_SHIFT) & (PTE_MASK & ~0x3FF);
        memset(&(get_lookup(pte_lookup)[pte_base]), 0x00, 0x1000);
    }
    else if(get_pde_entry(address)->frame == KMEM_POISON)
        return MMU_MAPPING_ERROR;

    // Check if we are trying to map to an already mapped address
    if(get_pte_entry(address)->p)
        return MMU_MAPPING_EXISTS;

    // This is an unmapped address, so map it
    get_pte_entry(address)->frame = mapping >> 12;
    status = mmu_change_attr(address, flags);
    if(status != 0)
        get_pte_entry(address)->frame = 0;
    return status;
}

int mmu_change_attr(void* address, uint32_t flags)
{
    // TODO: Check for PAT support
    uint8_t pat_index = 0;

    if( get_pde_entry(address)->p == 0)
        return MMU_MAPPING_ERROR;

    // Set up caching flags
    switch(flags & MMU_CACHE_MASK)
    {
        case MMU_CACHE_WB:  pat_index = 0b000; break;
        case MMU_CACHE_WT:  pat_index = 0b001; break;
        // case MMU_CACHE_UCO: pat_index = 0b010; break;
        case MMU_CACHE_UC:  pat_index = 0b011; break;
        // case MMU_CACHE_WC:  pat_index = 0b100; break;
        // case MMU_CACHE_WP:  pat_index = 0b111; break;
        default:
            return MMU_MAPPING_NOT_CAPABLE;
    }

    get_pte_entry(address)->p =  (flags & MMU_ACCESS_R) >> 0;
    get_pte_entry(address)->rw = (flags & MMU_ACCESS_W) >> 1;
    get_pte_entry(address)->su = (flags & MMU_ACCESS_USER) >> 3;
    get_pte_entry(address)->pwt = (pat_index >> 0) & 1;
    get_pte_entry(address)->pcd = (pat_index >> 1) & 1;
    get_pte_entry(address)->pat = (pat_index >> 2) & 1;
    invlpg(address);

    return 0;
}

bool mmu_unmap(void* address, bool erase)
{
    if( get_pde_entry(address)->p == 0 ||
        get_pte_entry(address)->p == 0) return false;

    get_pte_entry(address)->p = 0;

    if(erase)
    {
        mmu_change_attr(address, 0);
        get_pte_entry(address)->frame = KMEM_POISON >> 12;
    }

    invlpg(address);
    return true;
}

unsigned long mmu_get_mapping(void* address)
{
    if( get_pde_entry(address)->p == 0)
        return 0;

    return get_pte_entry(address)->frame << 12;
}

bool mmu_check_access(void* address, uint32_t flags)
{
    if( get_pde_entry(address)->p == 0)
        return false;

    uint32_t entry_flags = 0;
    entry_flags |= (get_pte_entry(address)->p  << 0);
    entry_flags |= (get_pte_entry(address)->rw << 1);
    entry_flags |= (                         1 << 2);       // i386 doesn't have NX in non-PAE mode
    entry_flags |= (get_pte_entry(address)->su << 3);

    return (entry_flags & flags) == flags;
}

void* mm_get_base()
{
    return (void*) MMU_BASE;
}

paging_context_t* mmu_create_context()
{
    // Create page context base
    unsigned long pdt_context = mm_alloc(1);

    // Map context to temporary address
    mmu_map((void*)temp_map_base, pdt_context, MMU_ACCESS_RW | MMU_CACHE_WB);
    memset((void*)temp_map_base, 0x00, 0x1000);

    // Copy relavent mappings to address space (Excluding temporary and recursive mapping)
    memcpy((uint8_t*)temp_map_base+2048, (uint8_t*)pde_lookup+2048, 2048-16);

    // Change recursive mapping entry
    page_entry_t *pde_temp_lookup = (page_entry_t*)temp_map_base;
    pde_temp_lookup[511].frame = pdt_context >> 12ULL;
    pde_temp_lookup[511].rw = 1;
    pde_temp_lookup[511].p = 1;

    paging_context_t *context = kmalloc(sizeof(paging_context_t));
    context->phybase = pdt_context;

    mmu_unmap((void*)temp_map_base, true);
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

    if(temp_context != addr_context)
    {
        // Change the context to the new one
        temp_context = addr_context;

        // Overwrite the temporary mapping entry
        pde_lookup[510].frame = temp_context->phybase >> 12ULL;
        invlpg(&(pde_lookup[510]));
    }

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
    asm volatile("movl %%eax, %%cr3\n\t"::
        "a"(addr_context->phybase) );
}
