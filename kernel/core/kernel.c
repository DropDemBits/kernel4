#include <stdint.h>
#include <mb2parse.h>

extern void halt();

void kmain()
{
	multiboot_parse();
	/*
	 * TODO: Sometimes our bootloader will fulfill our request. Search PCI
	 * devices for sutable device.
	 */

	if(mb_framebuffer_type == MULTIBOOT_FRAMEBUFFER_TYPE_RGB)
	{
		uint64_t *framebuffer = (uint64_t*) mb_framebuffer_addr;
		for(uint64_t i = 0; i < mb_framebuffer_width * mb_framebuffer_height; i++)
		{
			framebuffer[i] = 0xFFFFFFFFFFFFFFFF;
		}
	}
	else if(mb_framebuffer_type == MULTIBOOT_FRAMEBUFFER_TYPE_EGA_TEXT)
	{
		// Do Hello World
		uint16_t *framebuffer = (uint16_t*) mb_framebuffer_addr;
		framebuffer[0]  = 'H' | 0x0700;
		framebuffer[1]  = 'e' | 0x0700;
		framebuffer[2]  = 'l' | 0x0700;
		framebuffer[3]  = 'l' | 0x0700;
		framebuffer[4]  = 'o' | 0x0700;
		framebuffer[5]  = ' ' | 0x0700;
		framebuffer[6]  = 'W' | 0x0700;
		framebuffer[7]  = 'o' | 0x0700;
		framebuffer[8]  = 'r' | 0x0700;
		framebuffer[9]  = 'l' | 0x0700;
		framebuffer[10] = 'd' | 0x0700;
		framebuffer[11] = '!' | 0x0700;
	}

	halt();
}
