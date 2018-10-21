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

#include <common/mb2parse.h>
#include <common/io/keycodes.h>
#include <common/io/kbd.h>
#include <common/kshell/kshell.h>
#include <common/mm/mm.h>
#include <common/tty/tty.h>
#include <common/tty/fb.h>
#include <common/sched/sched.h>

#include <common/hal.h>
#include <common/util/kfuncs.h>

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

static char* input_buffer = KNULL;
static int index = 0;
static bool input_done = false;
static bool should_run = true;
static uint8_t shell_text_clr = 0x7;
static uint8_t shell_bg_clr = 0x0;
static tty_dev_t* tty;
static thread_t *test_wakeup = KNULL;
static thread_t *refresh_thread = KNULL;

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
    while(1)
    {
        if(tty->refresh_back)
            fb_fillrect(get_fb_address(), 0, 0, tty->width << 3, tty->height << 4, tty->current_palette[tty->default_colour.bg_colour]);
    
        tty_reshow_fb(tty, get_fb_address(), 0, 0);
        tty_make_clean(tty);
        sched_block_thread(STATE_SUSPENDED);
    }
}

static void print_log(uint16_t log_level)
{
    struct klog_entry* entry = (struct klog_entry*)(uintptr_t)get_klog_base();

    char buffer[256];

    while(entry->level != EOL)
    {
        if(entry->level >= log_level)
        {
            uint64_t timestamp_secs = entry->timestamp / 1000000000;
            uint64_t timestamp_ms = (entry->timestamp / 1000000) % 100000;

            if(!(entry->flags & KLOG_FLAG_NO_HEADER))
            {
                sprintf(buffer, "[%3llu.%05llu] (%4s): ", timestamp_secs, timestamp_ms, klog_get_name(entry->subsystem_id));
                tty_puts(tty, buffer);
            }

            for(uint16_t i = 0; i < entry->length; i++)
            {
                tty_putchar(tty, entry->data[i]);
            }
        }
        entry = (struct klog_entry*)((char*)entry + (entry->length + sizeof(struct klog_entry)));
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
        char kcode = kbd_read();
        char kchr = kbd_tochar(kcode);

        if(kcode == KEY_UP_ARROW)
        {
            tty_scroll(tty, -1);
        }
        else if(kcode == KEY_DOWN_ARROW)
        {
            if(tty->display_base < tty->draw_base)
                tty_scroll(tty, 1);
        }
        else if(kchr == '\t')
        {
            // Don't show a tab
            continue;
        }

        if(kcode && kchr)
        {
            // Don't put the charachter on screen if it is a backspace and it
            // is at the beginning of the index buffer
            if((kchr != '\b' && index < INPUT_SIZE) || kchr == '\n' || (kchr == '\b' && index > 0))
            {
                tty_scroll(tty, 0);
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
        puts("Kshell version 0.5\n");
        puts("Available commands (slashes = aliases, square brackets = arguments):");
        puts("\thelp/?:          \tShows this information");
        puts("\thelloworld/hw:   \tShows an example string");
        puts("\tclear/cls:       \tClears the screen");
        puts("\techo [thistext]: \tShows [thistext]");
        puts("\texit:            \tExits the console (reboot to bring back shell)");
        puts("\tfonttest:        \tShows all charachters supported by the current font");
        puts("\twakeup:          \tWake up a test thread to show a string");
        puts("\tsleep [time]:    \tSleeps the shell for [time] milliseconds");
        puts("\tklog [level]:    \tShows the kernel log from the specified log level");
        puts("\t                 \tand above");
        return true;
    } else if(is_command("exit", command))
    {
        should_run = false;
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
            for(int i = 0; i < FATAL; i++)
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
            log_level = DEBUG;

        print_log(log_level);
        return true;
    }

    return false;
}

void kshell_main()
{
    tty = kmalloc(sizeof(tty_dev_t));
    uint16_t* buffer = kmalloc(80*25*2*4);
    memset(buffer, 0, 80*25*2*4);

    tty_init(tty, 80, 25, buffer, 80*25*2*4, NULL);
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

    while(should_run)
    {
        shell_readline();
        if(!shell_parse())
        {
            tty_set_colours(tty, 0xC, 0x0);
            printf("%s: command not found\n", input_buffer);
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
