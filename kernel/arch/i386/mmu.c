#include <mm.h>

struct PageEntry {
	uint8_t p : 1;   // Present bit
	uint8_t rw : 1;  // Read/Write bit
	uint8_t su : 1;  // Supervisor/User bit
	uint8_t pwt : 1; // Page Write-through
	uint8_t pcd : 1; // Page cache disable
	uint8_t a : 1;   // Accessed bit
	uint8_t d : 1;   // Dirty bit
	uint8_t pat : 1; // Present bit
	uint8_t g : 1;   // Global
	uint8_t avl : 3; // Available for use
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
};

static page_entry_t *cr3;
static page_entry_t* const pde_lookup = (page_entry_t*) 0xFFFFF000;
static page_entry_t* const pte_lookup = (page_entry_t*) 0xFFC00000;

static void invlpg(linear_addr_t* address)
{
	asm volatile("invlpg (%%eax)" : "=a"(address));
}

void mmu_init()
{
	// Set CR3
	asm volatile("movl %%cr3, %%eax\n\t"
		: "=a"(cr3));
}

void mmu_map_address(linear_addr_t* address, physical_addr_t* mapping)
{
	uint32_t pde_off = ((linear_addr_t)address >> PDT_SHIFT) & 0x003FF;
	uint32_t pte_off = ((linear_addr_t)address >> PTT_SHIFT) & 0xFFFFF;

	// Check PDE Presence
	if(!pde_lookup[pde_off].p)
	{
		// Allocate a new PTE
		// We don't have a page frame allocator, so just return
		return;
	}

	// Check if we are trying to map to an already mapped address
	if(pte_lookup[pte_off].p)
	{
		// We are trying to map to such an address, so return
		return;
	}

	// This is an unmapped address, so map it
	pte_lookup[pte_off].p = 1;
	pte_lookup[pte_off].rw = 1;
	pte_lookup[pte_off].su = 0;
	pte_lookup[pte_off].frame = ((physical_addr_t)mapping >> 12);

	// Invalidate entry
	invlpg(address);
}
