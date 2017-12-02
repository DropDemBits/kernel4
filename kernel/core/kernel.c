#include <stdint.h>
#include <mm.h>
#include <vga.h>
#include <mb2parse.h>
#include <uart.h>

extern void halt();

size_t strlen(const char* str)
{
    size_t len = 0;
    while(str[len]) len++;
    return len;
}

serial_write(const char* str)
{
	uart_writestr(str, strlen(str));
}

void kmain()
{
	uart_init();
	serial_write("Hello!");
	multiboot_parse();
	mm_init();
	mmu_init();
	multiboot_reclaim();

	/*
	 * TODO: Sometimes our bootloader will not fulfill our request for a video
	 * device. Search PCI devices for sutable video device.
	 */

	linear_addr_t *framebuffer = get_fb_address();

	// Map framebuffer
	for(physical_addr_t off = 0;
		off <= (mb_framebuffer_width * mb_framebuffer_height * (mb_framebuffer_bpp / 8));
		off += 0x1000)
	{
		mmu_map_address((linear_addr_t*)((linear_addr_t)framebuffer + off),
					  (physical_addr_t*)(mb_framebuffer_addr + off));
	}

	if(mb_framebuffer_type == MULTIBOOT_FRAMEBUFFER_TYPE_RGB)
	{
		for(uint32_t i = 0; i < (mb_framebuffer_width * mb_framebuffer_height * (mb_framebuffer_bpp / 8)) >> 2; i++)
		{
			((uint32_t*)framebuffer)[i] = ((i & 0xFF) << 24) | ((i & 0xFF) << 16) | ((i & 0xFF) << 8) | ((i & 0xFF) << 0);
		}
	}
	else if(mb_framebuffer_type == MULTIBOOT_FRAMEBUFFER_TYPE_EGA_TEXT)
	{
		uint16_t* console = (uint16_t*) framebuffer;
		// Clear screen
		for(int i = 0; i < mb_framebuffer_width * mb_framebuffer_height; i++)
			console[i] = 0x0700;

		// Do Hello World
		console[0]  = 'H' | 0x0700;
		console[1]  = 'e' | 0x0700;
		console[2]  = 'l' | 0x0700;
		console[3]  = 'l' | 0x0700;
		console[4]  = 'o' | 0x0700;
		console[5]  = ' ' | 0x0700;
		console[6]  = 'W' | 0x0700;
		console[7]  = 'o' | 0x0700;
		console[8]  = 'r' | 0x0700;
		console[9]  = 'l' | 0x0700;
		console[10] = 'd' | 0x0700;
		console[11] = '!' | 0x0700;
	}

	halt();
}
