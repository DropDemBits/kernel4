#include <stdbool.h>
#include <mm.h>

enum {
	RTYPE_LOW,
	RTYPE_32BIT,
	RTYPE_64BIT,
};

struct mem_flags
{
	uint64_t present : 1;
	uint64_t type : 2;
	uint64_t reserved : 9;
} __attribute__((__packed__));
typedef struct mem_flags mem_flags_t;

struct mem_block
{
	struct mem_block* next;
	uint64_t* bitmap;
	uint64_t super_map;

	mem_flags_t flags;
	uint64_t base : 52;
} __attribute__((__packed__));
typedef struct mem_block mem_region_t;

static mem_region_t* region_list = KNULL;
static size_t total_blocks = 0;
static size_t frame_blocks = 0;

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
static mem_region_t* list_search_block(
	mem_region_t* list_head,
	uint64_t addr,
	mem_flags_t flag_match)
{
	// TODO: Abstract away into a macro
	mem_region_t* block = list_head;

	while(block != KNULL)
	{
		if(addr >= block->base && block->flags.type == flag_match.type) break;
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
	// TODO: Abstract away into a macro
	mem_region_t* block = list_head;

	while(block != KNULL)
	{
		if(block->super_map != 0xFFFFFFFFFFFFFFFF) break;
		block = block->next;
	}

	return block;
}

/*
 * Appends a new block to the list. Automatically allocates frames to the list
 * if needed.
 */
static void mm_append_block(mem_region_t* list_item, mem_region_t* block)
{
	list_insert_after(list_item, block);
	total_blocks++;
	if(++frame_blocks >= 64)
	{
		// TODO: Allocate new blocks
	}
}

// Bitmap utilities
typedef union
{
	struct
	{
		uint16_t bit_index32 : 5;
		uint16_t dword_index : 10;
		uint16_t sign32 : 1;
	};
	struct
	{
		uint16_t bit_index64 : 6;
		uint16_t qword_index : 9;
		uint16_t sign64 : 1;
	};
	uint16_t value;
} bm_index_t;

/*typedef union
{
	struct
	{
		uint16_t padding32 : 9;
		uint16_t bit_index32 : 6;
		uint16_t sign : 1;
	};
	struct
	{
		uint16_t padding64 : 9;
		uint16_t bit_index64 : 6;
		uint16_t sign : 1;
	};
} sbm_index_t;*/

typedef union
{
	struct
	{
		uint16_t padding : 9;
		uint16_t bit_index : 6;
		uint16_t sign : 1;
	};
	uint16_t value;
} sbm_index_t;

static void bm_set_bit(mem_region_t* mem_block, size_t index)
{
	if(mem_block == KNULL) return;
	if(index >= 32768)
	{
		// TODO: Alert our things
		return;
	}

	bm_index_t bm_index = {.value = index};
	sbm_index_t sbm_index = {.value = index};

	if(sizeof(uint64_t*) == 8)
	{
		mem_block->bitmap[bm_index.qword_index] |= (1 << bm_index.bit_index64);
		if(mem_block->bitmap[bm_index.qword_index] == ~0)
			mem_block->super_map |= (1 << sbm_index.bit_index);
	} else
	{
		mem_block->bitmap[bm_index.dword_index] |= (1 << bm_index.bit_index32);
		if(mem_block->bitmap[(bm_index.dword_index & 1) + 0] == ~0 &&
			mem_block->bitmap[(bm_index.dword_index & 1) + 1] == ~0)
			mem_block->super_map |= (1 << sbm_index.bit_index);
	}
}

static void bm_clear_bit(mem_region_t* mem_block, size_t index)
{
	if(mem_block == KNULL) return;
	if(index >= 32768)
	{
		// TODO: Alert our things
		return;
	}

	bm_index_t bm_index = {.value = index};
	sbm_index_t sbm_index = {.value = index};

	if(sizeof(uint64_t*) == 8)
	{
		mem_block->bitmap[bm_index.qword_index] &= ~(1 << bm_index.bit_index64);
		mem_block->super_map &= ~(1 << sbm_index.bit_index);
	} else
	{
		mem_block->bitmap[bm_index.dword_index] &= ~(1 << bm_index.bit_index32);
		mem_block->super_map &= ~(1 << sbm_index.bit_index);
	}
}

static uint8_t bm_get_bit(mem_region_t* mem_block, size_t index)
{
	if(mem_block == KNULL) return 1;
	if(index >= 32768)
	{
		// TODO: Alert our things
		return 1;
	}

	bm_index_t bm_index = {.value = index};
	if(sizeof(uint64_t*) == 8)
	{
		return (uint8_t) (mem_block->bitmap[bm_index.qword_index] >> (bm_index.bit_index64) & 0x1);
	} else
	{
		return (uint8_t) (mem_block->bitmap[bm_index.dword_index] >> (bm_index.bit_index32) & 0x1);
	}
}

static size_t bm_find_bit(mem_region_t* mem_block, uint8_t bit)
{
	bm_index_t index = {.value = 0xFFFF};

	// TODO: Bitsearch
	return (size_t)index.value;
}

void mm_init()
{
	// Now that we have our regions defined, map our region list to virt-mem
	asm("xchg %%bx, %%bx"::"a"(region_list));
}

void mm_add_region(physical_addr_t base, size_t length, uint32_t type)
{
	mem_region_t *tail = list_search_free(region_list);
	bool was_init_call = false;
	bool base_in_bda = false;
	bool clear_map = false;

	if(tail == KNULL)
	{
		was_init_call = true;

		// Set first 8K block in region for region tracking
		if(base < 0x1000)
		{
			// On PC-BIOS systems, the low 512B contains information we still
			// need, so we don't overwrite the memory yet.
			base += 0x1000;
			length -= 0x1000;
			base_in_bda = true;
		}
		region_list = (mem_region_t*)base;

		base += 0x2000;
		length -= 0x2000;

		//Set region to 0xFF
		for(size_t i = 0; i < 2048; i++)
		{
			((uint32_t*)region_list)[i] = 0xFFFFFFFF;
		}

		region_list->next = KNULL;
		region_list->base = base >> 27;
		region_list->super_map = 0;

		// Flags
		if(base < 0x1000000) region_list->flags.type = RTYPE_LOW;
		else if(base <= 0xFFFFFFFF) region_list->flags.type = RTYPE_32BIT;
		else region_list->flags.type = RTYPE_64BIT;
		region_list->flags.reserved = 0;
	}
	if(was_init_call)
	{
		tail = list_search_free(region_list);
		clear_map = true;
	} else if((base >> 27) > (tail->base >> 27))
	{
		//asm volatile("xchg %bx, %bx");
		if(type != 1 && type != 3) return;
		// TODO Allocate new block
		clear_map = true;
	}

	if(clear_map)
	{
		// Init bitmap
		tail->bitmap = (uint64_t*)(base - 0x1000);
		for(size_t i = 0; i < 512; i++)
		{
			tail->bitmap[i] = 0xFFFFFFFFFFFFFFFF;
		}
	}

	if(was_init_call)
	{
		// Set bits for allocated blocks
		if(base_in_bda) bm_set_bit(region_list, (base - 0x3000) >> 12);
		bm_set_bit(region_list, (base - 0x2000) >> 12);
		bm_set_bit(region_list, (base - 0x1000) >> 12);
	}

	// Set bits according to type
	for(size_t block = (base >> 27);;)
	{
		for(size_t bit = (base >> 12); bit < ((length >> 12) & 0x7FFF); bit++)
		{
			if(type == 1)
			{
				bm_clear_bit(tail, bit);
			}
			else bm_set_bit(tail, bit);
		}

		if(++block >= (length >> 27)) break;
		// TODO: Alloc new block
		asm("xchg %bx, %bx");
	}//*/
}
