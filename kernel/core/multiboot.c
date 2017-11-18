#include <multiboot.h>
#include <multiboot2.h>
#include <mm.h>
#include <types.h>

uptr_t multiboot_ptr = (void*)0;
size_t multiboot_magic = 0;

/* Multiboot info */
// Basic memory information
uint32_t mb_mem_lower;
uint32_t mb_mem_upper;

uint32_t mb_mods_count;
uint32_t mb_mods_addr;

// Framebuffer things
uint64_t mb_framebuffer_addr;
uint32_t mb_framebuffer_width;
uint32_t mb_framebuffer_height;
uint8_t mb_framebuffer_type;
uint8_t mb_framebuffer_bpp;

/* Forward Declerations */
void parse_mb1();
void parse_mb2();

void multiboot_parse()
{
	// Check which multiboot we're working with:

	switch (multiboot_magic) {
		case MULTIBOOT2_BOOTLOADER_MAGIC:
			parse_mb2();
			break;
		case MULTIBOOT_BOOTLOADER_MAGIC:
			parse_mb1();
		default:
			// Not loaded by multiboot loader
			break;
	}

}

/**
 * Parsing the Multiboot info structure is easy, just copy values over.
 */
void parse_mb1()
{
	multiboot_info_t* mb1 = (multiboot_info_t*)multiboot_ptr;
	uint32_t flags = mb1->flags;

	if(flags & MULTIBOOT_INFO_MEMORY)
	{
		mb_mem_lower = mb1->mem_lower;
		mb_mem_upper = mb1->mem_upper;
	}

	if(flags & MULTIBOOT_INFO_MODS)
	{
		// Module information
		mb_mods_count = mb1->mods_count;
		mb_mods_addr = mb1->mods_addr;
	}

	if(flags & MULTIBOOT_INFO_MEM_MAP)
	{
		// TODO Parse this for mm
	}

	if(flags & MULTIBOOT_INFO_FRAMEBUFFER_INFO)
	{
		mb_framebuffer_addr = mb1->framebuffer_addr;
		mb_framebuffer_width = mb1->framebuffer_width;
		mb_framebuffer_height = mb1->framebuffer_height;
		mb_framebuffer_type = mb1->framebuffer_type;
		mb_framebuffer_bpp = mb1->framebuffer_bpp;
	}

}

/**
 * Parsing the Multiboot2 info structure requires looping over all the tags
 */
void parse_mb2()
{
	// Check address alignment
	if(((size_t)multiboot_ptr) & 7)
	{
		// Our address is unaligned... Do something about that.
		return;
	}

	struct multiboot_tag *tag;

	for (tag = (struct multiboot_tag *) (multiboot_ptr + (8 * 8 / sizeof(uptr_t))); tag->type != MULTIBOOT_TAG_TYPE_END; tag = (struct multiboot_tag *) ((uint8_t *) tag + ((tag->size + 7) & ~7)))
	{
		switch (tag->type) {
			case MULTIBOOT_TAG_TYPE_BASIC_MEMINFO:
				mb_mem_lower = ((struct multiboot_tag_basic_meminfo*) tag)->mem_lower;
				mb_mem_upper = ((struct multiboot_tag_basic_meminfo*) tag)->mem_upper;
				break;
			case MULTIBOOT_TAG_TYPE_MODULE:
				mb_mods_addr = ((struct multiboot_tag_module *) tag)->mod_start;
				mb_mods_count++;
				break;
			case MULTIBOOT_TAG_TYPE_MMAP:
				// TODO: Parse mmap
			{
				multiboot2_memory_map_t *mmap;

				for (mmap = ((struct multiboot_tag_mmap *) tag)->entries;
				(uint8_t *) mmap < (uint8_t *) tag + tag->size;
				mmap = (multiboot2_memory_map_t *)((uint32_t) mmap + ((struct multiboot_tag_mmap *) tag)->entry_size))
				{
					mm_add_region(mmap->addr, mmap->len, mmap->type);
				}
			}
				break;
			case MULTIBOOT_TAG_TYPE_FRAMEBUFFER:
				mb_framebuffer_addr = ((struct multiboot_tag_framebuffer *) tag)->common.framebuffer_addr;
				mb_framebuffer_width = ((struct multiboot_tag_framebuffer *) tag)->common.framebuffer_width;
				mb_framebuffer_height = ((struct multiboot_tag_framebuffer *) tag)->common.framebuffer_height;
				mb_framebuffer_type = ((struct multiboot_tag_framebuffer *) tag)->common.framebuffer_type;
				mb_framebuffer_bpp = ((struct multiboot_tag_framebuffer *) tag)->common.framebuffer_bpp;
				break;
			default:
				break;
		}
	}
}
