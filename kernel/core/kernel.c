#include <stdint.h>
#include <mm.h>
#include <fb.h>
#include <hal.h>
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
	fb_init();
	multiboot_reclaim();
	hal_init();

	/*
	 * TODO: Sometimes our bootloader will not fulfill our request for a video
	 * device. Search PCI devices for sutable video device.
	 */

	uint32_t *framebuffer = (uint32_t*)get_fb_address();

	// Map framebuffer
	for(physical_addr_t off = 0;
		off <= (fb_info.width * fb_info.height * fb_info.bytes_pp);
		off += 0x1000)
	{
		mmu_map_address((linear_addr_t*)((linear_addr_t)framebuffer + off),
						(physical_addr_t*)(fb_info.base_addr + off));
	}

	if(fb_info.type == MULTIBOOT_FRAMEBUFFER_TYPE_RGB)
	{
		tty_add_output(FB_CONSOLE, (size_t)get_fb_address());

		// Clear screen
		fb_fillrect(framebuffer, 0, 0, fb_info.width, fb_info.height, 0x000000);
	}
	else if(fb_info.type == MULTIBOOT_FRAMEBUFFER_TYPE_EGA_TEXT)
	{
		tty_add_output(VGA_CONSOLE, (size_t)framebuffer);

		// Clear screen
		for(int i = 0; i < fb_info.width * fb_info.height; i++)
			((uint16_t*)framebuffer)[i] = 0x0700;
	}

	tty_prints("Je suis un test.\n");

	// Exception Handling Test
	//uint8_t* result = (uint8_t*)0xFFBFEEEE;
	//uart_writec(*result);

	hal_enable_interrupts();
	while(1)
	{
		tty_reshow();
		asm("hlt");
	}
}
