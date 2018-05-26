/**
 * Copyright (C) 2018 DropDemBits
 * 
 * This file is part of Kernel4.
 * 
 * Kernel4 is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * Kernel4 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with Kernel4.  If not, see <http://www.gnu.org/licenses/>.
 * 
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <common/mm.h>
#include <common/fb.h>
#include <common/hal.h>
#include <common/mb2parse.h>
#include <common/uart.h>
#include <common/tty.h>
#include <common/kfuncs.h>
#include <common/sched.h>
#include <common/tasks.h>
#include <common/ps2.h>
#include <common/kbd.h>
#include <common/vfs.h>
#include <common/tarfs.h>
#include <common/syscall.h>
#include <common/kshell/kshell.h>

extern uint32_t initrd_start;
extern uint32_t initrd_size;

void core_fini();

void idle_loop()
{
	while(1)
	{
		preempt_disable();
		if(tty_background_dirty())
		{
			fb_fillrect(get_fb_address(), 0, 0, fb_info.width, fb_info.height, 0);
		}
		tty_reshow();
		tty_make_clean();
		sched_gc();
		preempt_enable();
		sched_switch_thread();
	}
}

void low_priothread()
{
	while(1)
	{
		sched_gc();
		if(tty_background_dirty())
		{
			fb_fillrect(get_fb_address(), 0, 0, fb_info.width, fb_info.height, 0);
			tty_reshow();
			tty_make_clean();
		}

	}
}

extern void enter_usermode(uint64_t* register_state, uint64_t* entry_addr);
extern void usermode_code();

void usermode_entry()
{
	uint64_t* retaddr = (uint64_t*)0x400000;
	uint64_t* copy = (uint64_t*)&usermode_code;

	mmu_map(retaddr);
	memcpy(retaddr, copy, 4096);

	enter_usermode(sched_active_thread()->register_state, retaddr);
	while(1);
}

void time_nomnoms()
{
	while(1)
	{
		if(tty_background_dirty())
		{
			fb_fillrect(get_fb_address(), 0, 0, fb_info.width, fb_info.height, 0);
			tty_reshow();
			tty_make_clean();
		}
		tty_reshow();

		tty_set_colour(0x7, 0x0);
		putchar('p');
		sched_switch_thread();
	}
}

void a_print()
{
	while(1)
	{
		tty_set_colour(0xF, 0xC);
		putchar('a');
		// tty_reshow();
		sched_switch_thread();
	}
}

void b_print()
{
	while(1)
	{
		tty_set_colour(0x0, 0x2);
		putchar('b');
		// tty_reshow();
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
	mm_init();
	mmu_init();
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
	
	// Resume init in other thread
	process_t *p1 = process_create();
	thread_create(p1, (uint64_t*)idle_loop, PRIORITY_IDLE, "idleloop");
	thread_create(p1, (uint64_t*)core_fini, PRIORITY_KERNEL, "coreinit");

	preempt_enable();
	while(1) sched_switch_thread();
}

void core_fini()
{
	tty_prints("Initialising PS/2 controller\n");
	ps2_init();
	tty_prints("Initialising keyboard driver\n");
	kbd_init();
	tty_prints("Setting up system calls\n");
	syscall_init();

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
#endif

	tty_prints("Finished Initialisation\n");

	preempt_disable();
	process_t *p1 = process_create();
	thread_create(p1, (uint64_t*)low_priothread, PRIORITY_LOWER, "lowprio");
	thread_create(p1, (uint64_t*)kshell_main, PRIORITY_NORMAL, "kshell");
	thread_create(p1, (uint64_t*)usermode_entry, PRIORITY_NORMAL, "usermode");
	preempt_enable();

	// Now we are done, exit thread.
	sched_set_thread_state(sched_active_thread(), STATE_EXITED);
}
