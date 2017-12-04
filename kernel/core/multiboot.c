#include <types.h>
#include <mm.h>
#include <fb.h>
#include <tty.h>
#include <multiboot.h>
#include <multiboot2.h>

typedef multiboot2_memory_map_t mb2_mmap_t;
typedef multiboot_memory_map_t mb_mmap_t;

uptr_t multiboot_ptr = (void*)0;
size_t multiboot_magic = 0;
// 4KiB aligned pointer
size_t multiboot_base = 0;
size_t multiboot_size = 0;

/* Multiboot info */
// Basic memory information
uint32_t mb_mem_lower;
uint32_t mb_mem_upper;

uint32_t mb_mods_count;
uint32_t mb_mods_addr;

/* Forward Declerations */
void parse_mb1();
void parse_mb2();

void multiboot_parse()
{
	// Check which multiboot we're working with

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

/*
 * Reclaims memory occupied by multiboot info structures
 */
void multiboot_reclaim()
{
	mm_add_region(	multiboot_base,
					multiboot_size,
					MULTIBOOT_MEMORY_AVAILABLE);
}

/**
 * Parsing the Multiboot info structure is easy, just copy values over.
 */
void parse_mb1()
{
	multiboot_info_t* mb1 = (multiboot_info_t*)multiboot_ptr;
	uint32_t flags = mb1->flags;
	multiboot_base = ((size_t)mb1 + 0xFFF) & ~0xFFF;
	multiboot_size = 0x1000;

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

	if(flags & MULTIBOOT_INFO_FRAMEBUFFER_INFO)
	{
		fb_info.base_addr = mb1->framebuffer_addr;
		fb_info.width = mb1->framebuffer_width;
		fb_info.height = mb1->framebuffer_height;
		fb_info.type = mb1->framebuffer_type;
		fb_info.pitch = mb1->framebuffer_pitch;
		fb_info.bits_pp = mb1->framebuffer_bpp;
		fb_info.bytes_pp = mb1->framebuffer_bpp >> 3;

		if(fb_info.type == TYPE_INDEXED)
		{
			fb_info.palette_size = mb1->framebuffer_palette_num_colors;
			fb_info.palette_addr = mb1->framebuffer_palette_addr;
		}
		else if (fb_info.type == TYPE_RGB)
		{
			fb_info.red_position = mb1->framebuffer_red_field_position;
			fb_info.red_mask_size = mb1->framebuffer_red_mask_size;
			fb_info.green_position = mb1->framebuffer_green_field_position;
			fb_info.green_mask_size = mb1->framebuffer_green_mask_size;
			fb_info.blue_position = mb1->framebuffer_blue_field_position;
			fb_info.blue_mask_size = mb1->framebuffer_blue_mask_size;
		}
	}

	if(flags & MULTIBOOT_INFO_BOOT_LOADER_NAME)
	{
		tty_prints("Loaded by bootloader \"");
		tty_prints(mb1->boot_loader_name);
		tty_prints("\"\n");
	}

	// This needs to be last as to not overwrite the rest of multiboot things
	if(flags & MULTIBOOT_INFO_MEM_MAP)
	{
		mb_mmap_t* mmap = (mb_mmap_t*)mb1->mmap_addr;

		bool first_iter = true;
		while((size_t)mmap < mb1->mmap_addr + mb1->mmap_length) {
			mb2_mmap_t* actual = (mb2_mmap_t*)((uint32_t)mmap + 4);
			mm_add_region(actual->addr, actual->len, actual->type);
			if(first_iter)
			{
				// Reserve multboot info
				mm_add_region(	multiboot_base,
								multiboot_size,
								MULTIBOOT_MEMORY_RESERVED);
				first_iter = false;
			}

			mmap = (mb_mmap_t*) ((uint32_t)mmap + mmap->size + sizeof(mmap->size));
		}
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
	struct multiboot_tag_mmap *mmap_tag = KNULL;
	size_t info_size = 0;
	mb2_mmap_t *mmap;

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
				// Walk through later
				mmap_tag = ((struct multiboot_tag_mmap *) tag);
				break;
			case MULTIBOOT_TAG_TYPE_FRAMEBUFFER:
			{
				struct multiboot_tag_framebuffer *fb = ((struct multiboot_tag_framebuffer *) tag);
				fb_info.base_addr = fb->common.framebuffer_addr;
				fb_info.width = fb->common.framebuffer_width;
				fb_info.height = fb->common.framebuffer_height;
				fb_info.type = fb->common.framebuffer_type;
				fb_info.pitch = fb->common.framebuffer_pitch;
				fb_info.bits_pp = fb->common.framebuffer_bpp;
				fb_info.bytes_pp = fb->common.framebuffer_bpp >> 3;
				if(fb_info.type == TYPE_INDEXED)
				{
					fb_info.palette_size = fb->framebuffer_palette_num_colors;
					fb_info.palette_addr = (uint64_t)&(fb->framebuffer_palette);
				}
				else if (fb_info.type == TYPE_RGB)
				{
					fb_info.red_position = fb->framebuffer_red_field_position;
					fb_info.red_mask_size = fb->framebuffer_red_mask_size;
					fb_info.green_position = fb->framebuffer_green_field_position;
					fb_info.green_mask_size = fb->framebuffer_green_mask_size;
					fb_info.blue_position = fb->framebuffer_blue_field_position;
					fb_info.blue_mask_size = fb->framebuffer_blue_mask_size;
				}
			}
				break;
			case MULTIBOOT_TAG_TYPE_ELF_SECTIONS:
				break;
			case MULTIBOOT_TAG_TYPE_BOOT_LOADER_NAME:
				tty_prints("Loaded by bootloader \"");
				tty_prints(((struct multiboot_tag_string*)tag)->string);
				tty_prints("\"\n");
				break;
			default:
				break;
		}
		info_size += tag->size;
	}

	// Check if we have been passed a memory map
	if(mmap_tag == KNULL) return;

	// 4KiB align info size
	info_size = (info_size + (8 + 0xFFF)) & ~0xFFF;

	bool first_iter = true;

	for (mmap = mmap_tag->entries;
		(uint8_t *) mmap < ((uint8_t *) mmap_tag + mmap_tag->size);
		mmap = (mb2_mmap_t *)((uint64_t) mmap + mmap_tag->entry_size))
	{
		mm_add_region(mmap->addr, mmap->len, mmap->type);
		if(first_iter)
		{
			// Reserve multiboot info region
			multiboot_base = (physical_addr_t)multiboot_ptr & ~0xFFF;
			multiboot_size = info_size;
			mm_add_region(	multiboot_base,
							multiboot_size,
							MULTIBOOT_MEMORY_RESERVED);

			first_iter = false;
		}
	}
}
