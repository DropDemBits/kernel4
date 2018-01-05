#include <stdint.h>
#include <stdio.h>
#include <ctype.h>
#include <mm.h>
#include <fb.h>
#include <hal.h>
#include <mb2parse.h>
#include <uart.h>
#include <tty.h>
#include <kfuncs.h>
#include <sched.h>
#include <tasks.h>

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

void sample_thing()
{
	printf("hello");
}

void kmain()
{
	tty_init();
	uart_init();
	tty_prints("Hello World!\n");
	multiboot_parse();
	mmu_init();
	mm_init();
	fb_init();
	multiboot_reclaim();
	hal_init();
	sched_init();

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
		mmu_map_direct((linear_addr_t*)((linear_addr_t)framebuffer + off),
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

	hal_enable_interrupts();

	// TODO: Wrap into a separate test file
	uint8_t* alloc_test = kmalloc(16);
	printf("Alloc test: %#p\n", (uintptr_t)alloc_test);
	kfree(alloc_test);

	// Part 1: allocation
	uint32_t* laddr = (uint32_t*)0xF0000000;
	physical_addr_t* addr = mm_alloc(1);
	printf("PAlloc test: Addr0 (%#p)\n", (uintptr_t)addr);

	// Part 2: Mapping
	mmu_map_direct(laddr, addr);
	*laddr = 0xbeefb00f;
	printf("At Addr1 direct map (%#p): %#lx\n", laddr, *laddr);

	// Part 3: Remapping
	mmu_unmap(laddr);
	mmu_map(laddr);
	printf("At Addr1 indirect map (%#p): %#lx\n", laddr, *laddr);
	if(*laddr != 0xbeefb00f) kpanic("PAlloc test failed (laddr is %#lx)", laddr);

	process_t *p1 = process_create();
	process_t *p2 = process_create();
	thread_create(p1, sample_thing);
	thread_create(p2, sample_thing);
	thread_create(p1, sample_thing);
	thread_create(p2, sample_thing);

	while(1)
	{
		printf("(%d, %d) ", sched_active_process()->pid, sched_active_thread()->tid);
		sched_switch_thread();

		if(tty_background_dirty())
		{
			fb_fillrect(framebuffer, 0, 0, fb_info.width, fb_info.height, 0);
			tty_make_clean();
		}
		tty_reshow();
		asm("hlt");
	}
}
