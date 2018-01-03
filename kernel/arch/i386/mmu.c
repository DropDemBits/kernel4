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

static page_entry_t *cr3;
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

			if((uintptr_t)address < 0x80000000)
				entry->su = 1;
			else
				entry->su = 0;
		}

		invlpg(address);
		return new_page_entry;
	}

	return false;
}

isr_retval_t pf_handler(struct intr_stack *frame)
{
	struct PageError *page_error = (struct PageError*)frame->err_code;
	uint32_t address = frame->cr2;

	if(frame->cr2 >= 0xE0000000 && frame->cr2 < 0xF0000000)
	{
		// Our visual output devices are bork'd, so fall back onto serial.
		tty_add_output(FB_CONSOLE, (size_t)KNULL);
		tty_add_output(VGA_CONSOLE, (size_t)KNULL);
	}

	kpanic_intr(frame, "Error: Page fault at %#p (error code %x)", address, page_error);
	// This shouldn't be reached
	return ISR_HANDLED;
}

void mmu_init()
{
	// Set CR3
	asm volatile("movl %%cr3, %%eax\n\t"
		: "=a"(cr3));
	isr_add_handler(14, (isr_t)pf_handler);
}

int mmu_map_direct(linear_addr_t* address, physical_addr_t* mapping)
{
	if(mapping == KNULL || address == KNULL) return -1;

	// Check PDE Presence
	if(check_and_map_entry(get_pde_entry(address), address))
	{
		uint64_t pte_base = ((uintptr_t)address >> PTT_SHIFT) & (PTE_MASK & ~0x2FF);
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

	if((uintptr_t)address < 0x800000000)
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
	if(	get_pde_entry(address)->p == 0 ||
		get_pte_entry(address)->p == 0) return;

	get_pte_entry(address)->p = 0;
	mm_free((physical_addr_t*)(get_pte_entry(address)->frame << 12), 1);
	get_pte_entry(address)->frame = KMEM_POISON;

	invlpg(address);
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
