#include <string.h>

#include <mm.h>
#include <tty.h>
#include <kfuncs.h>
#include <stack_state.h>
#include <idt.h>

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

static page_entry_t *cr3;
static page_entry_t* const pml4e_lookup = (page_entry_t*) 0xFFFFFFFFFFFFF000;
static page_entry_t* const pdpe_lookup  = (page_entry_t*) 0xFFFFFFFFFFE00000;
static page_entry_t* const pde_lookup   = (page_entry_t*) 0xFFFFFFFFC0000000;
static page_entry_t* const pte_lookup   = (page_entry_t*) 0xFFFFFF8000000000;

static void invlpg(linear_addr_t* address)
{
	asm volatile("invlpg (%%rax)" : "=a"(address));
}

static page_entry_t* get_pml4e_entry(linear_addr_t* address)
{
	uint64_t pml4e_off = ((uintptr_t)address >> PML4_SHIFT) & PML4E_MASK;

	return &(pml4e_lookup[pml4e_off]);
}

static page_entry_t* get_pdpe_entry(linear_addr_t* address)
{
	uint64_t pdpe_off = ((uintptr_t)address >> PDPT_SHIFT) & PDPE_MASK;

	return &(pdpe_lookup[pdpe_off]);
}

static page_entry_t* get_pde_entry(linear_addr_t* address)
{
	uint64_t pde_off = ((uintptr_t)address >> PDT_SHIFT) & PDE_MASK;

	return &(pde_lookup[pde_off]);
}

static page_entry_t* get_pte_entry(linear_addr_t* address)
{
	uint64_t pte_off = ((uintptr_t)address >> PTT_SHIFT) & PTE_MASK;

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
			{
				entry->frame = KMEM_POISON;
				return false;
			}

			entry->frame = frame >> 12;
			entry->frame = frame >> 12;
			entry->p = 1;
			entry->rw = 1;
			entry->xd = 1;
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
	struct PageError *page_error = (struct PageError*)frame->err_code;
	uint64_t address = frame->cr2;

	if(frame->cr2 >= 0xFFFFE00000000000 && frame->cr2 < 0xFFFFF00000000000)
	{
		// Our visual output devices are bork'd, so fall back onto serial.
		tty_add_output(FB_CONSOLE, (size_t)KNULL);
		tty_add_output(VGA_CONSOLE, (size_t)KNULL);
	}

	kpanic_intr(frame, "Error: Page fault at %#llx (error code %x)", address, page_error);
}

void mmu_init()
{
	// Set CR3
	asm volatile("movq %%cr3, %%rax\n\t":
		"=a"(cr3));
	isr_add_handler(14, (isr_t)pf_handler);
}

int mmu_map_direct(linear_addr_t* address, physical_addr_t* mapping)
{
	if(mapping == KNULL || address == KNULL) return -1;

	// TODO: Add PML5 support
	// Check PML4E Presence
	if(check_and_map_entry(get_pml4e_entry(address), address))
	{
		uint64_t pdpe_base = ((uintptr_t)address >> PDPT_SHIFT) & (PDPE_MASK & ~0x1FF);
		memset(&(pdpe_lookup[pdpe_base]), 0x00, 0x1000);
	}
	else if(get_pml4e_entry(address)->frame == KMEM_POISON)
		return -1;

	// Check PDPE Presence
	if(check_and_map_entry(get_pdpe_entry(address), address))
	{
		uint64_t pde_base = ((uintptr_t)address >> PDT_SHIFT) & (PDE_MASK & ~0x1FF);
		memset(&(pde_lookup[pde_base]), 0x00, 0x1000);
	}
	else if(get_pdpe_entry(address)->frame == KMEM_POISON)
		return -1;

	// Check PDE Presence
	if(check_and_map_entry(get_pde_entry(address), address))
	{
		uint64_t pte_base = ((uintptr_t)address >> PTT_SHIFT) & (PTE_MASK & ~0x1FF);
		memset(&(pte_lookup[pte_base]), 0x00, 0x1000);
	}
	else if(get_pde_entry(address)->frame == KMEM_POISON)
		return -1;

	// Check if we are trying to map to an already mapped address
	if(get_pte_entry(address)->p)
		return -1;

	// This is an unmapped address, so map it
	get_pte_entry(address)->xd = 1;
	get_pte_entry(address)->rsv = 0;
	get_pte_entry(address)->p = 1;
	get_pte_entry(address)->rw = 1;
	get_pte_entry(address)->frame = ((physical_addr_t)mapping >> 12);

	if((uintptr_t)address < 0xFFFF800000000000)
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

void mmu_unmap(linear_addr_t* address)
{
	if(	get_pml4e_entry(address)->p == 0 ||
		get_pdpe_entry(address)->p == 0 ||
		get_pde_entry(address)->p == 0 ||
		get_pte_entry(address)->p == 0) return;

	get_pte_entry(address)->p = 0;
	get_pte_entry(address)->rw = 0;
	get_pte_entry(address)->su = 0;
	mm_free((physical_addr_t*)(get_pte_entry(address)->frame << 12), 1);
	get_pte_entry(address)->frame = KMEM_POISON;

	invlpg(address);
}

bool mmu_is_usable(linear_addr_t* address)
{
	if(	get_pml4e_entry(address)->p &&
		get_pdpe_entry(address)->p &&
		get_pde_entry(address)->p &&
		get_pte_entry(address)->p) return true;
	return false;
}

linear_addr_t* mm_get_base()
{
	return (linear_addr_t*) 0xFFFF880000000000;
}
