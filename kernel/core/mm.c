#include <mm.h>

struct mem_flags
{
	uint64_t allocated : 1;
	uint64_t reserved : 63;
};
typedef struct mem_flags mem_flags_t;

struct mem_region
{
	uint64_t* next;
	uint64_t base;
	uint64_t length;
	mem_flags_t flags;
};
typedef struct mem_region mem_region_t;

static mem_region_t* region_list = KNULL;

// List utilities (TODO: Abstract away into a new file)
/*
 * Gets the last item in the list
 */
static mem_region_t* list_get_tail(mem_region_t* list_head)
{
	mem_region_t* last_item = list_head;

	if(last_item == KNULL) return KNULL;
	while(last_item->next != KNULL) last_item = (mem_region_t*)last_item->next;

	return last_item;
}

/*
 * Inserts a block after the specified item in a list
 */
static void list_insert_after(mem_region_t* list_item, mem_region_t* insert)
{
	if(list_item == KNULL || insert == KNULL) return;
	// Are we inserting at the list tail?
	if(list_item->next == KNULL)
	{
		// Append item to list tail
		list_item->next = insert;
		return;
	}

	// Insert between two items
	mem_region_t* next_item = list_item->next;
	list_item->next = insert;
	/*
	 * TODO: We are trusting that the insert provided is safe. Can this
	 * assumption bring about a security flaw?
	 */
	list_get_tail(insert)->next = next_item;
}

/*
 * Searches list for a memory block which contains the address and matching
 * flags.
 * Returns the block which matches the requirements, or KNULL if none was found
 */
static mem_region_t* list_search_block(mem_region_t* list_head, uint64_t addr, mem_flags_t flag_match)
{
	// TODO: Abstract away into a define
	mem_region_t* block = list_head;

	while(block != KNULL)
	{
		if(addr >= block->base && block->flags.allocated == flag_match.allocated) break;
		block = block->next;
	}

	return block;
}

/*
 * Searches list for the first free memory block
 * Returns the first free block, or KNULL if none was found
 */
static mem_region_t* list_search_free(mem_region_t* list_head)
{
	// TODO: Abstract away into a define
	mem_region_t* block = list_head;

	while(block != KNULL)
	{
		if(block->flags.allocated == 0) break;
		block = block->next;
	}

	return block;
}

void mm_init()
{
	// Now that we have our regions defined, map our region list to virt-mem
	asm("xchg %%bx, %%bx"::"a"(region_list));
}

void mm_add_free_region(physical_addr_t base, size_t length)
{
	mem_region_t *tail = list_get_tail(region_list);
	if(tail == KNULL)
	{
		// Set first 4K block in region for region tracking
		if(base < 0x1000)
		{
			// On PC-BIOS systems, the low 512B contains information we still
			// need, so we don't overwrite the memory yet.
			base += 0x1000;
			length -= 0x1000;
		}
		region_list = (mem_region_t*)base;

		base += 0x1000;
		length -= 0x1000;

		//Set region to 0xFF
		for(size_t i = 0; i < 1024; i++)
		{
			((uint32_t*)region_list)[i] = 0xFFFFFFFF;
		}

		region_list->next = KNULL;
		region_list->base = base;
		region_list->length = length;
		region_list->flags.allocated = 0;
		region_list->flags.reserved = 0;
	} else
	{
		tail->next = (uint64_t*)tail+4;
		tail = (mem_region_t*)tail->next;
		tail->next = KNULL;
		tail->base = base;
		tail->length = length;
		tail->flags.allocated = 0;
		tail->flags.reserved = 0;
	}

}
