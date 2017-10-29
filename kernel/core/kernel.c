#include <stdint.h>

extern void halt();

static uint32_t set_eax(uint32_t value)
{
	return value;
}

void kmain()
{
	/*int *framebuffer = (int*) 0xE0000000;
	for(int i = 0; i < 640 * 480; i++)
	{
		framebuffer[i] = 0xFFFFFFFF;
	}*/
	uint32_t test = set_eax(0xFEFEB00F);
	halt();
}
