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

#include <common/acpi.h>
#include <common/hal.h>
#include <common/hal/timer.h>
#include <common/util/kfuncs.h>
#include <common/util/klog.h>
#include <common/mb2parse.h>
#include <common/syscall.h>
#include <common/ata/ata.h>
#include <common/io/kbd.h>
#include <common/io/pci.h>
#include <common/io/ps2.h>
#include <common/io/uart.h>
#include <common/kshell/kshell.h>
#include <common/mm/liballoc.h>
#include <common/mm/mm.h>
#include <common/sched/sched.h>
#include <common/tasks/tasks.h>
#include <common/fs/vfs.h>
#include <common/fs/tarfs.h>
#include <common/fs/ttyfs.h>
#include <common/tty/fb.h>
#include <common/tty/tty.h>
#include <common/usb/usb.h>
#include <common/elf.h>

// TODO: Move definitions to more appropriate files
#include <arch/iobase.h>

extern uint32_t initrd_start;
extern uint32_t initrd_size;
extern unsigned long long tswp_counter;
extern struct thread_queue run_queue;

void core_fini();

void idle_loop()
{
    while(1)
        intr_wait();
}

void info_display()
{
    const int x_off = 0;
    const int y_off = 25 << 4;
    const int buffer_size = 80*5*2;

    uint16_t* buffer = kmalloc(buffer_size);
    tty_dev_t* tty = kmalloc(sizeof(tty_dev_t));
    uint64_t last_swap_count = 0;
    uint64_t swap_timer = timer_read_counter(0) + 1000000000;
    uint64_t draw_time = 0;
    uint64_t print_time = 0;
    uint64_t next_print_time = 0;
    uint64_t reshow_time = 0;
    bool show_times = false;
    bool clean_back = false;
    int thread_count = 0;

    tty_init(tty, 80, 5, buffer, buffer_size, NULL);
    tty_set_colours(tty, 0xF, 0x0);

    while(1)
    {
        char buf[256];
        int current_thread_count = 0;

        next_print_time = timer_read_counter(0);
        tty_set_cursor(tty, 0, 0, false);

        // Clear background
        if(clean_back)
            fb_fillrect(get_fb_address(), x_off, y_off + 32, tty->width << 3, (tty->height << 4) - 32, tty->current_palette[tty->default_colour.bg_colour]);
        clean_back = false;

        // Divider
        for(int i = 0; i < tty->width; i++)
            tty_putchar(tty, '-');

        // Uptime
        uint64_t now = timer_read_counter(0) / 1000000;
        sprintf(buf, "Uptime: %lld.%03lld\n", now / 1000, now % 1000);
        tty_puts(tty, buf);

        // Swap counter
        sprintf(buf, "Thread swaps/s: %lld\n", last_swap_count);
        tty_puts(tty, buf);

        // Active thread
        sprintf(buf, "[%s] ", sched_active_thread()->name);
        tty_puts(tty, buf);

        // Thread queue proper
        thread_t* node = run_queue.queue_head;
        while(node != KNULL)
        {
            if(current_thread_count > 8)
            {
                tty_puts(tty, "...");
                break;
            }

            sprintf(buf, "%s ", node->name);
            tty_puts(tty, buf);
            node = node->next;
            current_thread_count++;
        }
        tty_putchar(tty, '\n');

        if(current_thread_count != thread_count)
        {
            thread_count = current_thread_count;
            clean_back = true;
        }

        if(swap_timer < timer_read_counter(0))
        {
            swap_timer = timer_read_counter(0) + 1000000000;
            if(last_swap_count > tswp_counter)
                clean_back = true;
            last_swap_count = tswp_counter;
            tswp_counter = 0;
        }

        if(show_times)
        {
            sprintf(buf, "Draw times: (p:%uns r:%uns t:%uns)", print_time, reshow_time, draw_time);
            tty_puts(tty, buf);
        }

        print_time = timer_read_counter(0) - next_print_time;
        reshow_time = timer_read_counter(0);

        // Force reshow all
        tty->refresh_back = true;
        tty_reshow_fb(tty, get_fb_address(), x_off, y_off);
        tty_make_clean(tty);
        tty_clear(tty, true);
        reshow_time = timer_read_counter(0) - reshow_time;
        draw_time = print_time + reshow_time;
        sched_sleep_ms(10);
        show_times = true;
    }
}

void main_fb_init()
{
    void* framebuffer = get_fb_address();

    // Map framebuffer (and any extra bits of it)
    unsigned long fb_size = (fb_info.width * fb_info.height * fb_info.bytes_pp + 0xFFF) & ~0xFFF;

    int status = 0;

    for(unsigned long off = 0; off <= fb_size; off += 0x1000)
    {
        status = mmu_map(framebuffer + off, fb_info.base_addr + off, MMU_ACCESS_RW | MMU_CACHE_WC);

        // Retry using WB caching if mapping failed
        if(status != 0 && status == MMU_MAPPING_NOT_CAPABLE)
            status = mmu_map(framebuffer + off, fb_info.base_addr + off, MMU_ACCESS_RW | MMU_CACHE_WB);

        if(status != 0)
            break;
    }

    if(status != 0 || !mmu_check_access(get_fb_address(), MMU_ACCESS_RW))
        return;

    if(fb_info.type == MULTIBOOT_FRAMEBUFFER_TYPE_RGB)
    {
        // Clear screen
        fb_clear();
    }
    else if(fb_info.type == MULTIBOOT_FRAMEBUFFER_TYPE_EGA_TEXT)
    {
        // Clear screen
        for(uint32_t i = 0; i < fb_info.width * fb_info.height; i++)
            ((uint16_t*)framebuffer)[i] = 0x0700;
    }
}

extern process_t init_process;
void kmain()
{
    uart_init();
    klog_early_init();
    klog_logln(LVL_INFO, "Initialising UART");
    uart_writec('1');
    klog_logln(LVL_INFO, "Parsing Multiboot info");
    multiboot_parse();
    uart_writec('2');
    klog_logln(LVL_INFO, "Initialising MM");
    mm_early_init();
    mmu_init();
    mm_init();
    uart_writec('3');

    klog_logln(LVL_INFO, "Initialising Framebuffer");
    fb_init();
    main_fb_init();
    acpi_early_init();
    uart_writec('4');

    klog_logln(LVL_INFO, "Initialising HAL");
    hal_init();
    uart_writec('5');
    
    // Resume init in other thread
    klog_logln(LVL_INFO, "Starting up threads for init");
    tasks_init("init", (void*)core_fini);
    thread_create(&init_process, (void*)idle_loop, PRIORITY_IDLE, "idle_thread", NULL);

    // sched_init depends on init_process
    klog_logln(LVL_INFO, "Initialising Scheduler");
    sched_init();
    klog_logln(LVL_DEBUG, "Entering threaded init");

    klog_init();

    while(1)
    {
        sched_lock();
        sched_switch_thread();
        sched_unlock();
    }
}

void walk_dir(struct dnode* dir, int level)
{
    struct dirent dirent;
    int i = 0;

    if(dir == NULL)
        return;

    while(vfs_readdir(dir, i++, &dirent) != NULL)
    {
        // Don't show current or parent directory dots
        if(!strncmp(dirent.name, ".", 1) || !strncmp(dirent.name, "..", 1))
            continue;

        klog_log(LVL_INFO, "");
        for(int j = 0; j < level; j++)
            klog_logf(LVL_INFO, KLOG_FLAG_NO_HEADER, "|  ");

        klog_loglnf(LVL_INFO, KLOG_FLAG_NO_HEADER, "|_ %s ", dirent.name);

        if((to_inode(dir)->type & 7) == VFS_TYPE_DIRECTORY)
        {
            // Walk through subdirectories
            struct dnode* node = vfs_find_dir(dir, dirent.name);
            if(node != NULL)
                walk_dir(node, level + 1);
        }
    }
}

void core_fini()
{
    klog_logln(LVL_INFO, "Setting up system calls");
    syscall_init();

    acpi_init();

    kbd_init();
    // usb_init();
    ata_init();
    pci_init();

    // Needs to be after (usb emulation must be disabled)
    ps2_init();

    uint8_t eject_command[] = {0x1B /* START STOP UNIT */, 0x00, 0x00, 0x00, /* LoEJ */ 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    uint8_t read_command[]  = {0xA8 /* READ(12) */, 0x00, /* LBA */ 0x00, 0x00, 0x00, 0x01, /* Sector Count */ 0x00, 0x00, 0x00, 0x01, /**/ 0x00, 0x00};
    uint8_t* transfer_buffer = kmalloc(4096);
    int err_code = 0;

    klog_logln(LVL_INFO, "Command: ATAPI READ(12):", err_code);
    err_code = atapi_send_command(2, (uint16_t*)read_command, (uint16_t*)transfer_buffer, 4096, TRANSFER_READ, false, false);
    klog_logln(LVL_INFO, "ErrCode (%d)", err_code);

    // Dump contents
    for(size_t i = 0; i < 2048 && !err_code; i++)
    {
        klog_logf(LVL_DEBUG, (i % 16 > 0) ? KLOG_FLAG_NO_HEADER : 0, "%02x ", (transfer_buffer[i] & 0xFF), (transfer_buffer[i] >> 8));
        if(i % 16 == 15)
            klog_logc(LVL_DEBUG, '\n');
    }

    klog_logln(LVL_INFO, "Command: ATAPI START STOP UNIT (LoEJ):");
    err_code = atapi_send_command(2, (uint16_t*)eject_command, transfer_buffer, 4096, TRANSFER_READ, false, false);
    klog_logln(LVL_INFO, "ErrCode (%d)", err_code);

    klog_logln(LVL_INFO, "Command: ATA READ");
    err_code = pata_do_transfer(0, 1, (uint16_t*)transfer_buffer, 1, TRANSFER_READ, false, false);
    klog_logln(LVL_INFO, "ErrCode (%d)", err_code);

    // Dump contents
    for(size_t i = 0; i < 512 && !err_code; i++)
    {
        klog_logf(LVL_INFO, (i % 16 > 0) ? KLOG_FLAG_NO_HEADER : 0, "%02x ", (transfer_buffer[i] & 0xFF), (transfer_buffer[i] >> 8));
        if(i % 16 == 15)
            klog_logc(LVL_INFO, '\n');
    }

    struct vfs_mount *root;
    if(initrd_start != 0xDEADBEEF)
    {
        klog_logln(LVL_INFO, "Setting up initrd");
        
        // Map initrd to temporary region
        for(size_t off = 0; off < PAGE_ROUNDUP(initrd_size) && off < INITRD_SIZE; off += PAGE_SIZE)
            mmu_map((void*)(INITRD_BASE + off), initrd_start + off, MMU_FLAGS_DEFAULT);

        struct fs_instance* tarfs = tarfs_init((void*)INITRD_BASE, initrd_size);
        klog_logln(LVL_INFO, "Mounting initrd:");
        vfs_mount(tarfs, "/");
        root = vfs_get_mount("/");

        klog_logln(LVL_INFO, "Mounting ttyfs:");
        struct fs_instance* ttyfs = ttyfs_create();
        vfs_mount(ttyfs, "/dev");

        klog_logln(LVL_INFO, "Walking dir tree:");
        walk_dir(root->instance->root, 0);
    }

    // TODO: Wrap into a separate test file
#ifdef ENABLE_TESTS
    uint8_t* alloc_test = kmalloc(16);
    klog_logln(LVL_INFO, "Alloc test: %#p", (uintptr_t)alloc_test);
    kfree(alloc_test);

    // Part 1: allocation
    uint32_t* laddr = (uint32_t*)0xF0000000;
    unsigned long addr = mm_alloc(1);
    klog_logln(LVL_INFO, "PAlloc test: Addr0 (%#p)", (uintptr_t)addr);

    // Part 2: Mapping
    mmu_map(laddr, addr, MMU_ACCESS_RW);
    *laddr = 0xbeefb00f;
    klog_logln(LVL_INFO, "At Addr1 direct map (%#p): %#lx", laddr, *laddr);

    // Part 3: Remapping
    mmu_unmap(laddr, true);
    mmu_map(laddr, mm_alloc(1), MMU_ACCESS_RW);
    klog_logln(LVL_INFO, "At Addr1 indirect map (%#p): %#lx", laddr, *laddr);
    if(*laddr != 0xbeefb00f) kpanic("PAlloc test failed (laddr is %#lx)", laddr);

    {
        klog_logln(LVL_INFO, "VFS-TEST");
        struct inode *blorg;
        struct inode *blorg_aaa;

        blorg = vfs_finddir(root, "blorg/");
        blorg = vfs_finddir(root, "blorg");
        vfs_finddir(blorg, "aaa/bbb");
        vfs_finddir(blorg, "aaa/cc");
        vfs_finddir(root, "blorg/aaa");
        blorg_aaa = vfs_finddir(root, "/blorg/aaa");
        vfs_finddir(root, "/bork/aaa");
        vfs_finddir(root, "/blorg/aa");
        klog_logln(LVL_INFO, "VFS-TEST-DONE");
    }
#endif

    klog_logln(LVL_INFO, "Finished Kernel Initialisation");

    taskswitch_disable();
    process_t *p1 = process_create("tui_process");
    thread_create(p1, (void*)kshell_main, PRIORITY_NORMAL, "kshell", NULL);
    thread_create(p1, (void*)info_display, PRIORITY_NORMAL, "info_thread", NULL);
    taskswitch_enable();

    // Now we are done, exit thread.
    sched_terminate();
}
