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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <common/hal.h>
#include <common/util/kfuncs.h>
#include <common/mb2parse.h>
#include <common/syscall.h>
#include <common/ata/ata.h>
#include <common/io/kbd.h>
#include <common/io/pci.h>
#include <common/io/ps2.h>
#include <common/io/uart.h>
#include <common/kshell/kshell.h>
#include <common/mm/mm.h>
#include <common/sched/sched.h>
#include <common/tasks/tasks.h>
#include <common/fs/tarfs.h>
#include <common/fs/vfs.h>
#include <common/tty/fb.h>
#include <common/tty/tty.h>

extern uint32_t initrd_start;
extern uint32_t initrd_size;

void core_fini();

void idle_loop()
{
    while(1)
    {
        intr_wait();
    }
}


extern void enter_usermode(void* thread, unsigned long entry_addr);
extern void usermode_code();

void usermode_entry()
{
    unsigned long retaddr = 0x400000;

    mmu_map(retaddr);
    memcpy((void*)retaddr, &usermode_code, 4096);

    enter_usermode((void*)(sched_active_thread()), retaddr);
    while(1);
}

extern unsigned long long tswp_counter;
extern struct thread_queue run_queue;
void info_display()
{
    const char* switches = "Swaps/s: ";
    int last_len = 0;

    while(1)
    {
        char buf[256];

        // Swaps / s
        fb_puts(get_fb_address(), 0, 26 << 4, switches);
        ulltoa(tswp_counter, buf, 10);
        for(int i = 0; i < strlen(buf); i++)
        {
            fb_fill_putchar(get_fb_address(), (strlen(switches) + i) << 3, 26 << 4, buf[i], 0xFFFFFFFF, 0x0);
        }

        int posX = 0;
        int posY = 27 << 4;

        // Active thread
        fb_fill_putchar(get_fb_address(), posX, posY, '[', 0xFFFFFFFF, 0);
        posX += 8;
        for(int i = 0; i < strlen(sched_active_thread()->name); i++)
        {
            fb_fill_putchar(get_fb_address(), posX, posY, sched_active_thread()->name[i], ~0, 0);
            posX += 8;
            if(posX > fb_info.width)
            {
                posY += 16;
                posX = 0;
            }
        }
        fb_fill_putchar(get_fb_address(), posX, posY, ']', 0xFFFFFFFF, 0);
        posX += 8;
        fb_fill_putchar(get_fb_address(), posX, posY, ' ', 0xFFFFFFFF, 0);
        posX += 8;

        // Print queue proper
        thread_t* node = run_queue.queue_head;
        while(node != KNULL)
        {
            for(int i = 0; i < strlen(node->name); i++)
            {
                fb_fill_putchar(get_fb_address(), posX, posY, node->name[i], ~0, 0);
                posX += 8;
                if(posX > fb_info.width)
                {
                    posY += 16;
                    posX = 0;
                }
            }
            fb_fill_putchar(get_fb_address(), posX, posY, ' ', 0xFFFFFFFF, 0);
            posX += 8;
            if(posX > fb_info.width)
            {
                posY += 16;
                posX = 0;
            }
            node = node->next;
            if(posY > 28 << 4)
            {
            }
        }

        // Clear old pixels
        if(posX < last_len)
            fb_fillrect(get_fb_address(), posX, posY, last_len - posX, 16, 0);
        last_len = posX;

        tswp_counter = 0;

        ulltoa(timer_read_counter(0) / 1000000, buf, 10);

        for(int i = 0; i < strlen(buf); i++)
        {
            fb_fill_putchar(get_fb_address(), i << 3, 28 << 4, buf[i], ~0, 0);
        }
        sched_sleep_ms(1000);
    }
}

extern process_t init_process;
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

    unsigned long framebuffer = (unsigned long)get_fb_address();

    // Map framebuffer
    for(unsigned long off = 0;
        off <= (fb_info.width * fb_info.height * fb_info.bytes_pp);
        off += 0x1000)
    {
        mmu_map_direct(framebuffer + off, fb_info.base_addr + off);
    }

    if(fb_info.type == MULTIBOOT_FRAMEBUFFER_TYPE_RGB)
    {
        tty_add_output(FB_CONSOLE, (unsigned long)get_fb_address());

        // Clear screen
        // fb_fillrect(framebuffer, 0, 0, fb_info.width, fb_info.height, 0x000000);
        fb_clear();
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
    
    // Resume init in other thread
    tty_prints("Starting up threads for init\n");
    tasks_init("init", (void*)core_fini);
    thread_create(&init_process, (void*)idle_loop, PRIORITY_IDLE, "idle_thread");

    // sched_init depends on init_process
    tty_prints("Initialising Scheduler\n");
    sched_init();

    sched_print_queues();
    tty_reshow();

    hal_enable_interrupts();
    while(1)
    {
        sched_lock();
        sched_switch_thread();
        sched_unlock();
    }
}

void core_fini()
{
    tty_prints("Initialising PS/2 controller\n");
    ps2_init();
    kbd_init();
    tty_prints("Setting up system calls\n");
    syscall_init();

    ata_init();
    pci_init();

    uint8_t eject_command[] = {0x1B /* START STOP UNIT */, 0x00, 0x00, 0x00, /* LoEJ */ 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    uint8_t read_command[]  = {0xA8 /* READ(12) */, 0x00, /* LBA */ 0x00, 0x00, 0x00, 0x01, /* Sector Count */ 0x00, 0x00, 0x00, 0x01, /**/ 0x00, 0x00};
    uint16_t* transfer_buffer = kmalloc(4096);
    int err_code = 0;

    printf("HEY\n");
    err_code = atapi_send_command(2, read_command, transfer_buffer, 4096, TRANSFER_READ, false, false);
    printf("OHH (%d)\n", err_code);
    printf("HEY\n");
    err_code = atapi_send_command(2, eject_command, transfer_buffer, 4096, TRANSFER_READ, false, false);
    printf("OHH (%d)\n", err_code);

    for(int i = 0; i < 1024; i++)
    {
        printf("%x ", transfer_buffer[i]);
        if((i & 0x1F) == 0x1F)
            putchar('\n');
        if((i & 0x3F) == 0x3F)
            tty_reshow();
    }

    // TODO: Wrap into a separate test file
#ifdef ENABLE_TESTS
    uint8_t* alloc_test = kmalloc(16);
    printf("Alloc test: %#p\n", (uintptr_t)alloc_test);
    kfree(alloc_test);

    // Part 1: allocation
    uint32_t* laddr = (uint32_t*)0xF0000000;
    unsigned long addr = mm_alloc(1);
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

    taskswitch_disable();
    process_t *p1 = process_create();
    thread_create(p1, (void*)kshell_main, PRIORITY_NORMAL, "kshell");
    thread_create(p1, (void*)info_display, PRIORITY_NORMAL, "info_thread");
    thread_create(process_create(), (uint64_t*)usermode_entry, PRIORITY_NORMAL, "usermode");
    taskswitch_enable();

    // Now we are done, exit thread.
    sched_terminate();
}
