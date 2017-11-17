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
	uint64_t frame : 36;

	uint8_t rsv : 4; // Reserved bits
	uint8_t ign : 7; // Ignored (Available) bits
	uint8_t pk : 4;  // Protection Key
	uint8_t xd : 1;  // NX bit
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

void mmu_init()
{
	// Set CR3
	asm volatile("movq %%cr3, %%rax\n\t": "=a"(cr3));
}

void mmu_map_address(linear_addr_t* address, physical_addr_t* mapping)
{
	// TODO: Add PML5 support
	uint64_t pml4e_off = ((linear_addr_t)address >> PML4_SHIFT) & 0x00000000001FF;
	uint64_t pdpe_off  = ((linear_addr_t)address >> PDPT_SHIFT) & 0x000000003FFFF;
	uint64_t pde_off   = ((linear_addr_t)address >> PDT_SHIFT)  & 0x0000007FFFFFF;
	uint64_t pte_off   = ((linear_addr_t)address >> PTT_SHIFT)  & 0x0000FFFFFFFFF;

	// Check PML4E Presence
	if(!pml4e_lookup[pml4e_off].p)
	{
		// Allocate a new PDPT
		// We don't have a page frame allocator, so just return
		return;
	}

	// Check PDPE Presence
	if(!pdpe_lookup[pdpe_off].p)
	{
		// Allocate a new PD
		// We don't have a page frame allocator, so just return
		return;
	}

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
