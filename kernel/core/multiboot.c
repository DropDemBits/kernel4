#include <multiboot.h>
#include <multiboot2.h>
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

}
