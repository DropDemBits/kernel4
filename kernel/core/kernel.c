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
#include <ps2.h>
#include <kbd.h>
#include <keycodes.h>
#include <vfs.h>
#include <tarfs.h>

extern uint32_t initrd_start;
extern uint32_t initrd_size;
bool refresh_needed = false;

void idle_thread()
{
	while(1)
	{
		if(tty_background_dirty())
		{
			fb_fillrect(get_fb_address(), 0, 0, fb_info.width, fb_info.height, 0);
			tty_make_clean();
		}
		tty_reshow();
		sched_switch_thread();
	}
}

void refresh_thread()
{
	while(1)
	{
		if(refresh_needed)
		{
			if(tty_background_dirty())
			{
				if(fb_info.type == MULTIBOOT_FRAMEBUFFER_TYPE_RGB)
				{
					fb_fillrect(get_fb_address(), 0, 0, fb_info.width, fb_info.height, 0);
				} else
				{
					for(int i = 0; i < fb_info.width * fb_info.height; i++)
						((uint16_t*)get_fb_address())[i] = 0x0700;
				}
				tty_make_clean();
			}
			tty_reshow();
			refresh_needed = false;
		}
		sched_switch_thread();
	}
}

void ly_thread()
{
	tty_set_colour(0xF, 0x0);
	for(unsigned char c = ' '; c <= 0x7F; c++)
	{
		tty_printchar(c);
		if(c % 16 == 15) tty_printchar('\n');
	}

	const char* test_string = "Hello_World!Clrs!\n";
	uint8_t colour_index = 0;

	while(*test_string)
	{
		tty_set_colour(colour_index, 0x10-colour_index++);
		tty_printchar(*(test_string++));
	}
	tty_set_colour(0xa, 0x0);

	while(1)
	{
		char key = kbd_read();
		if(key)
			tty_printchar(kbd_tochar(key));
		refresh_needed = true;
		sched_switch_thread();
	}
}

void kmain()
{
	tty_init();
	tty_prints("Initialising UART\n");
	uart_init();
	tty_prints("Parsing Multiboot info\n");
	multiboot_parse();
	tty_prints("Initialising MM\n");
	mmu_init();
	mm_init();
	tty_prints("Initialising Framebuffer\n");
	fb_init();
	multiboot_reclaim();

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
	tty_reshow();

	tty_prints("Initialising HAL\n");
	hal_init();
	hal_enable_interrupts();
	tty_prints("Initialising Scheduler\n");
	sched_init();
	tty_prints("Initialising PS/2 controller\n");
	ps2_init();
	tty_prints("Initialising keyboard driver\n");
	kbd_init();

	// TODO: Wrap into a separate test file
#ifdef ENABLE_TESTS
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
#endif

	if(initrd_start != 0xDEADBEEF)
	{
		puts("VFS-TEST");
		vfs_mount(tarfs_init((void*)initrd_start, initrd_size), "/");

		uint8_t *buffer = kmalloc(257);
		vfs_inode_t *root = vfs_getrootnode("/");
		struct vfs_dirent *dirent;
		int i = 0;

		memset(buffer, 0, 257);
		while((dirent = vfs_readdir(root, i++)) != KNULL)
		{
			vfs_inode_t* node = vfs_finddir(root, dirent->name);
			printf("%s ", dirent->name);
			switch (node->type & 0x7) {
				case VFS_TYPE_DIRECTORY:
					puts("(Directory)");
					break;
				default:
				{
					ssize_t len = vfs_read(node, 0, 256, buffer);
					if(len < 0) continue;
					buffer[len] = '\0';
					printf("(Read Len %d):\n%s\n", len, buffer);
				}
			}
		}
		kfree(buffer);
	}

	preempt_disable();
	process_t *p1 = process_create();
	process_t *p2 = process_create();
	thread_create(p1, (uint64_t*)idle_thread, PRIORITY_IDLE);
	/*
	 * The refresh thread is on the same priority as the others as there isn't
	 * conditional wakeups.
	 */
	thread_create(p2, (uint64_t*)refresh_thread, PRIORITY_HIGH);
	thread_create(p2, (uint64_t*)ly_thread, PRIORITY_HIGH);
	preempt_enable();

	// Now we are done, go to new thread.
	while(1) sched_switch_thread();
}
