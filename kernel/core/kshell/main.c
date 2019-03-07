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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include <common/acpi.h>
#include <common/mb2parse.h>
#include <common/io/keycodes.h>
#include <common/io/kbd.h>
#include <common/kshell/kshell.h>
#include <common/mm/mm.h>
#include <common/mm/liballoc.h>
#include <common/tty/tty.h>
#include <common/tty/fb.h>
#include <common/sched/sched.h>
#include <common/elf.h>

#include <common/hal.h>
#include <common/util/kfuncs.h>
#include <common/io/uart.h>
#include <common/fs/ttyfs.h>

/*
 * KSHELL: Basic command line interface
 *
 * Available commands:
 * help (?): Prints help information
 * helloworld (hw): Prints helloworld in various colours V
 * echo: Echos following command arguments V
 * clear (cls): Clears screen V
 */

#define INPUT_SIZE 255
#define ARG_DELIM (" \t")
#define SHELL_WIDTH 80
#define SHELL_HEIGHT 25
#define SHELL_SCREEN_SIZE (SHELL_WIDTH * SHELL_HEIGHT)
#define SHELL_SCREENS 64

static char* input_buffer = KNULL;
static int index = 0;
static bool input_done = false;
static bool should_run = true;
static uint8_t shell_text_clr = 0x7;
static uint8_t shell_bg_clr = 0x0;
static tty_dev_t* tty;
static thread_t *test_wakeup = KNULL;
static thread_t *refresh_thread = KNULL;

extern void enter_usermode(thread_t* thread, void* entry_addr);
extern uint32_t initrd_start;
extern uint32_t initrd_size;
extern process_t init_process;

void program_launch(const char* path)
{
    klog_logln(LVL_INFO, "Launching program %s", path);

    struct vfs_mount* mount = vfs_get_mount("/");
    struct dnode* dnode = vfs_find_dir(mount->instance->root, path);

    if(dnode == NULL)
    {
        klog_logln(LVL_ERROR, "Error: %s doesn't exist", path);
        sched_terminate();
    }
    
    struct inode* inode = dnode->inode;
    struct elf_data* elf_data;
    int errno = 0;

    klog_logln(LVL_DEBUG, "Parsing elf file");
    vfs_open(inode, VFSO_RDONLY);
    errno = elf_parse(inode, &elf_data);
    if(errno)
    {
        klog_logln(LVL_ERROR, "Error parsing elf file (ec %d)", errno);
        vfs_close(inode);
        sched_terminate();
    }

    // Validation is done, load the program
    void* entry_point = elf_data->entry_point;

    klog_logln(LVL_DEBUG, "Copying elf data");
    for(size_t i = 0; i < elf_data->phnum; i++)
    {
        struct elf_phdr* proghead = &elf_data->phdrs[i];

        if(proghead->p_type != PT_LOAD)
            continue;   // Only concerned with loding segments right now

        // Map all the pages
        uint32_t flags = proghead->p_flags;
        
        // Flip flag around (RWX -> XWR) and add read flag by default
        flags = ((flags & PF_X) << 2) | (flags & PF_W) | MMU_ACCESS_R;

        for(size_t off = 0; off < PAGE_ROUNDUP(proghead->p_memsz); off += 0x1000)
        {
            // Initially map pages to RW
            mmu_map((void*)((proghead->p_vaddr + off) & ~PAGE_MASK), mm_alloc(1), MMU_FLAGS_DEFAULT | MMU_ACCESS_USER);
        }

        // Copy data from the file
        vfs_read(elf_data->file, proghead->p_offset, proghead->p_filesz, (void*)proghead->p_vaddr);

        for(size_t off = 0; off < PAGE_ROUNDUP(proghead->p_memsz); off += 0x1000)
        {
            // Set as the final type
            mmu_change_attr((void*)(proghead->p_vaddr + off), flags | MMU_ACCESS_USER | MMU_CACHE_WB);
            klog_logln(LVL_DEBUG, "FlgsSet: %x", flags | MMU_ACCESS_USER | MMU_CACHE_WB);
        }
    }

    // Last cleanup
    elf_put(elf_data);
    klog_logln(LVL_DEBUG, "Done code copy");
    enter_usermode(sched_active_thread(), entry_point);
}

void wakeup_task()
{
    while(1)
    {
        sched_block_thread(STATE_SUSPENDED);
        tty_puts(tty, "\nI'M AWAKE NOW\n");
    }
}

void refresh_task()
{
    uint64_t next_refresh_time = 0;
    const uint64_t refresh_rate = 1000000000 / 60;

    while(1)
    {
        if(timer_read_counter(0) < next_refresh_time)
            goto keep_waiting;
        // Don't update too fast (helps prevent tearing)
        next_refresh_time = timer_read_counter(0) + refresh_rate;

        if(tty->refresh_back)
            fb_fillrect(get_fb_address(), 0, 0, tty->width << 3, tty->height << 4, tty->current_palette[tty->default_colour.bg_colour]);
    
        tty_reshow_fb(tty, get_fb_address(), 0, 0);
        tty_make_clean(tty);

        keep_waiting:
        sched_block_thread(STATE_SUSPENDED);
    }
}

static void print_log(uint16_t log_level)
{
    struct klog_entry* entry = (struct klog_entry*)(uintptr_t)get_klog_base();

    char buffer[128];

    while(entry->level != EOL)
    {
        if(entry->level >= log_level)
        {
            uint64_t timestamp_secs = entry->timestamp / 1000000000;
            uint64_t timestamp_ms = (entry->timestamp / 1000000) % 100000;

            if((entry->flags & KLOG_FLAG_NO_HEADER) != KLOG_FLAG_NO_HEADER)
            {
                sprintf(buffer, "[%3llu.%05llu]: ", timestamp_secs, timestamp_ms);
                tty_puts(tty, buffer);
            }

            for(uint16_t i = 0; i < entry->length; i++)
                tty_putchar(tty, entry->data[i]);
        }
        entry = (struct klog_entry*)((char*)entry + (entry->length + sizeof(struct klog_entry)));
    }
}

static void print_tree(process_t* process, int level)
{
    // Recursively print process tree
    if(process == KNULL)
        return;
    
    // First print processes (\ )
    for(int i = 0; i < level; i++)
        printf("|   ");
    printf("\\- %s\n", process->name);

    // Print the children
    process_t* child = process->child;
    while(child != KNULL)
    {
        print_tree(child, level + 1);
        child = child->sibling;
    }

    // Then threads (| )
    thread_t* thread = process->threads;
    while(thread != KNULL)
    {
        for(int i = 0; i < level; i++)
            printf("|   ");
        printf("|- %s\n", thread->name);
        thread = thread->sibling;
    }
}

static void request_refresh()
{
    sched_unblock_thread(refresh_thread);
}

static void shell_readline()
{
    printf("> ");
    input_done = false;
    index = 0;

    while(1)
    {
        char keycode = kbd_read();
        char kchr = kbd_tochar(keycode);

        // Keycode stuffs
        switch(keycode)
        {
            case KEY_UP_ARROW:
                tty_scroll(tty, -1, false);
                break;
            case KEY_DOWN_ARROW:
                tty_scroll(tty, 1, false);
                break;

            case KEY_PG_UP:
                // Scroll up an entire screen
                tty_scroll(tty, -SHELL_HEIGHT, false);
                break;
            case KEY_PG_DOWN:
                // Scroll down an entire screen
                tty_scroll(tty, SHELL_HEIGHT, false);
                break;
        }

        if(kchr == '\t')
        {
            // Don't show a tab
            continue;
        }

        if(keycode && kchr)
        {
            // Don't put the charachter on screen if it is a backspace and it
            // is at the beginning of the index buffer
            if((kchr != '\b' && index < INPUT_SIZE) || kchr == '\n' || (kchr == '\b' && index > 0))
            {
                tty_scroll(tty, 0, false);
                putchar(kchr);
            }

            if(kchr == '\n')
            {
                input_done = true;
                break;
            } else if(kchr == '\b' && index > 0)
            {
                input_buffer[--index] = '\0';
                continue;
            }

            if(kchr != '\b' && index < INPUT_SIZE)
            {
                input_buffer[index++] = kchr;
                input_buffer[index] = '\0';
            }
        }

        request_refresh();
    }
}

static bool is_command(const char* str, char* buffer)
{
    return strcmp(str,buffer) == 0;
}

extern void arch_reboot();

static bool shell_parse()
{
    if(!index) return true; // Nothing to parse

    // Skip argument delimiters
    input_buffer += strspn(input_buffer, ARG_DELIM);

    char* saveptr;
    char* command = strtok_r(input_buffer, ARG_DELIM, &saveptr);
    if(command == NULL) return true; // Nothing to parse

    if(is_command("echo", command))
    {
        printf("%s\n", strspn(saveptr, ARG_DELIM) + saveptr);
        return true;
    } else if(is_command("clear", command) || is_command("cls", command))
    {
        tty_clear(tty, true);
        return true;
    } else if(is_command("helloworld", command) || is_command("hw", command))
    {
        const char* string = "Hello, World! I am a test statement...\n";
        int clrindex = 0;

        for(size_t i = 0; i < strlen(string); i++)
        {
            if(string[i] != '\n')
            {
                tty_set_colours(tty, clrindex, ~(clrindex));
                clrindex++;
                clrindex &= 0xF;
            } else
            {
                tty_set_colours(tty, shell_text_clr, shell_bg_clr);
            }
            putchar(string[i]);
        }

        return true;
    } else if(is_command("help", command) || is_command("?", command))
    {
        puts("Kshell version 0.6.1\n");
        puts("Available commands (slashes = aliases, square brackets = arguments):");
        puts("\thelp/?:          \tShows this information");
        puts("\thelloworld/hw:   \tShows an example string");
        puts("\tclear/cls:       \tClears the screen");
        puts("\techo [thistext]: \tShows [thistext]");
        puts("\tfonttest:        \tShows all charachters supported by the current font");
        puts("\twakeup:          \tWake up a test thread to show a string");
        puts("\tsleep [time]:    \tSleeps the shell for [time] milliseconds");
        puts("\tklog [level]:    \tShows the kernel log from the specified log level");
        puts("\t                 \tand above");
        puts("\tshutdown [exit]: \tShuts down the computer");
        puts("\treboot:          \tReboots the computer");
        puts("\tps:              \tPrints out a list of all processes and threads");
        return true;
    } else if(is_command("fonttest", command))
    {
        for(unsigned int i = 0; i < 256; i++)
        {
            if(i == '\t' || i == '\n' || i == '\r' || i == '\b') putchar('\0');
            else putchar(i);

            if(i % 16 == 15) putchar('\n');
        }
        return true;
    } else if(is_command("wakeup", command))
    {
        if(test_wakeup != KNULL)
            sched_unblock_thread(test_wakeup);
        return true;
    } else if(is_command("sleep", command))
    {
        char* sleep_time = strtok_r(NULL, ARG_DELIM, &saveptr);
        long int sleep_for = 0;
        
        if(sleep_time != NULL) 
            sleep_for = atol(sleep_time);

        if(sleep_for < 0)
        {
            puts("sleep: Sleep time can't be negative!");

            // Parsing was correct, but the args weren't
            return true;
        }

        printf("Sleeping for %ld milliseconds\n", sleep_for);
        sched_sleep_ms(sleep_for);
        return true;
    } else if(is_command("klog", command))
    {
        char* level_names[] = {"debug", "info", "warn", "error", "fatal"};
        char* log_level_str = strtok_r(NULL, ARG_DELIM, &saveptr);
        long int log_level = 0;
        
        if(log_level_str != NULL) 
        {
            for(int i = 0; i < LVL_FATAL; i++)
            {
                if(strnicmp(level_names[i], log_level_str, strlen(level_names[i])) == 0)
                {
                    log_level = i + 1;
                    break;
                }
            }

            if(log_level == 0)
            {
                printf("Unknown log level: %s", log_level_str);
                return true;
            }
        }

        if(log_level == 0)
            log_level = LVL_DEBUG;

        print_log(log_level);
        return true;
    } else if(is_command("shutdown", command) || is_command("exit", command))
    {
        // Print shutdown message
        tty_set_colours(tty, EGA_BRIGHT_YELLOW, EGA_BLACK);
        printf("Goodbye");
        request_refresh();
        sched_sleep_ms(500);

        // Enter sleep state
        ACPI_STATUS Status = acpi_enter_sleep(5);
        if(ACPI_FAILURE(Status))
        {
            putchar('?');
            tty_set_colours(tty, shell_text_clr, shell_bg_clr);
            putchar('\n');
            printf("An error occurred while trying to shutdown: %s\n", AcpiFormatException(Status));
        }
        return true;
    } else if(is_command("reboot", command))
    {
        // Print reboot message
        tty_set_colours(tty, EGA_BRIGHT_YELLOW, EGA_BLACK);
        printf("Goodbye");
        request_refresh();
        sched_sleep_ms(500);

        // Reboot the system
        ACPI_STATUS Status = acpi_reboot();
        if(ACPI_FAILURE(Status))
        {
            putchar('?');
            tty_set_colours(tty, shell_text_clr, shell_bg_clr);
            putchar('\n');
            printf("An error occurred while trying to reboot: %s\n", AcpiFormatException(Status));
        }

        // Specialty
        arch_reboot();
        return true;
    }
    else if(is_command("ps", command))
    {
        // Print the process tree from init
        print_tree(&init_process, 0);
        return true;
    }

    // Try loading a program
    struct vfs_mount* mount = vfs_get_mount("/");
    struct dnode* file = vfs_find_dir(mount->instance->root, command);

    // Placeholder until fork & exec are implemented in usermode
    if(file == NULL || (to_inode(file)->type & 7) != VFS_TYPE_FILE)
    {
        // File doesn't exist or isn't a file
        return false;
    }

    process_t* proc = process_create("program");
    thread_create(proc, (void*)program_launch, PRIORITY_NORMAL, "thread", command);

    return true;
}

void kshell_main()
{
    uint16_t* buffer = kmalloc(SHELL_SCREEN_SIZE * SHELL_SCREENS * 2);

    tty = kmalloc(sizeof(tty_dev_t));
    memset(buffer, 0, SHELL_SCREEN_SIZE * SHELL_SCREENS * 2);

    tty_init(tty, SHELL_WIDTH, SHELL_HEIGHT, buffer, SHELL_SCREEN_SIZE * SHELL_SCREENS * 2, NULL);
    tty_select_active(tty);

    refresh_thread = thread_create(
        sched_active_process(),
        (uint64_t*)refresh_task,
        PRIORITY_HIGHER,
        "refresh_thread",
        NULL);

    test_wakeup = thread_create(
        sched_active_process(),
        (uint64_t*)wakeup_task,
        PRIORITY_NORMAL,
        "test_wakeup",
        NULL);

    tty_set_colours(tty, 0xF, 0x0);
    printf("Welcome to K4!\n");
    tty_set_colours(tty, 0x7, 0x0);

    input_buffer = kmalloc(INPUT_SIZE+1);
    memset(input_buffer, 0x00, INPUT_SIZE+1);

    // Add our tty to the list of ttys (temporary)
    ttyfs_add_tty((struct ttyfs_instance*)(vfs_get_mount("/dev")->instance), tty, "tty1");

    while(should_run)
    {
        shell_readline();
        if(!shell_parse())
        {
            tty_set_colours(tty, 0xC, 0x0);
            printf("%s: executable file or command not found\n", input_buffer);
            puts("Try 'help' or '?' for a list of available commands");
            tty_set_colours(tty, shell_text_clr, shell_bg_clr);
        }

        request_refresh();
    }

    puts("kshell is exiting, nothing left to do");

    // Cleanup stuff
    tty_reshow_fb(tty, get_fb_address(), 0, 0);
    tty_make_clean(tty);

    // Free remaining objects
    kfree(tty->buffer_base);
    kfree(tty);

    // Goodbye
    sched_terminate();
}
