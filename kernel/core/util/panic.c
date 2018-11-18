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

#include <stdio.h>
#include <string.h>

#include <common/hal.h>
#include <common/io/uart.h>
#include <common/tty/tty.h>
#include <common/tty/fb.h>
#include <common/util/kfuncs.h>

extern void __attribute__((noreturn)) halt();
extern char early_klog_buffer[];

tty_dev_t temp_tty = {};
static char tbuf[80*25*2];

static void reshow_log()
{
    struct klog_entry* entry = (struct klog_entry*)(uintptr_t)get_klog_base();
    tty_dev_t* tty = &temp_tty;
    char buffer[128];

    if(!klog_is_init())
    {
        entry = (struct klog_entry*)early_klog_buffer;
    }
    
    tty_init(tty, 80, 25, tbuf, 80*25*2, NULL);

    while(entry->level != EOL)
    {
        uint64_t timestamp_secs = entry->timestamp / 1000000000;
        uint64_t timestamp_ms = (entry->timestamp / 1) % 1000000000;

        if(!(entry->flags & KLOG_FLAG_NO_HEADER))
        {
            sprintf(buffer, "[%3llu.%05llu] (%5s): ", timestamp_secs, timestamp_ms, klog_get_name(entry->subsystem_id));
            uart_writestr(buffer, strlen(buffer));
            tty_puts(tty, buffer);
        }

        for(uint16_t i = 0; i < entry->length; i++)
        {
            uart_writec(entry->data[i]);
            tty_putchar(tty, entry->data[i]);
            
            if(entry->data[i] == '\n')
                uart_writec('\r');
        }
        
        next:
        entry = (struct klog_entry*)((char*)entry + (entry->length + sizeof(struct klog_entry)));
    }

    if(mmu_check_access((uintptr_t)get_fb_address(), MMU_ACCESS_RW))
        tty_reshow_fb(tty, get_fb_address(), 0, 0);
}

void __attribute__((noreturn)) kpanic(const char* message_string, ...)
{
    va_list args;
    va_start(args, message_string);
    kvpanic(message_string, args);
    va_end(args);
}

void __attribute__((noreturn)) kpanic_intr(struct intr_stack *stack, const char* message_string, ...)
{
    va_list args;
    va_start(args, message_string);
    kvpanic_intr(stack, message_string, args);
    va_end(args);
}

void __attribute__((noreturn)) kvpanic(const char* message_string, va_list args)
{
    if(klog_is_init())
    {
        klog_logln(0, FATAL, "Exception occured in kernel:");
        klog_loglnv(0, FATAL, message_string, args);
    }
    else
    {
        klog_early_logln(FATAL, "Exception occured in kernel:");
        klog_early_loglnv(FATAL, message_string, args);
    }

    reshow_log();
    halt();
}

void __attribute__((noreturn)) kvpanic_intr(struct intr_stack *stack, const char* message_string, va_list args)
{
    if(klog_is_init())
    {
        klog_logln(0, FATAL, "Exception occured in kernel:");
        klog_loglnv(0, FATAL, message_string, args);
    }
    else
    {
        klog_early_logln(FATAL, "Exception occured in kernel:");
        klog_early_loglnv(FATAL, message_string, args);
    }
    dump_registers(stack);

    reshow_log();
    halt();
}
