#include <stdint.h>
#include <mm.h>
#include <vga.h>
#include <mb2parse.h>
#include <uart.h>
#include <tty.h>

extern void halt();

size_t strlen(const char* str)
{
    size_t len = 0;
    while(str[len]) len++;
    return len;
}

void serial_write(const char* str)
{
	uart_writestr(str, strlen(str));
}

void kmain()
{
	tty_init();
	uart_init();
	tty_prints("Hello World!\n");
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
		tty_add_output(VGA_CONSOLE, (size_t)console);
		// Clear screen
		for(int i = 0; i < mb_framebuffer_width * mb_framebuffer_height; i++)
			console[i] = 0x0700;
	}

	tty_prints("Je suis un test.\n");

	uint8_t* result = (uint8_t*)0xFFBFEEEE;
	uart_writec(*result);

	halt();
}
