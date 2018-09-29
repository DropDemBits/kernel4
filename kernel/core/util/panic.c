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
#include <common/hal.h>
#include <common/tty/tty.h>
#include <common/util/kfuncs.h>

extern void __attribute__((noreturn)) halt();
extern char early_klog_buffer[];

static void reshow_log()
{
    struct klog_entry* entry = (struct klog_entry*)get_klog_base();

    if(!klog_is_init())
    {
        entry = (struct klog_entry*)early_klog_buffer;
    }

    while(entry->level != EOL)
    {
        uint64_t timestamp_secs = entry->timestamp / 1000000000;
        uint64_t timestamp_ms = (entry->timestamp / 1000000) % 100000;

        printf("[%3llu.%05llu] (%5s): ", timestamp_secs, timestamp_ms, klog_get_name(entry->subsystem_id));

        for(uint16_t i = 0; i < entry->length; i++)
            tty_printchar(entry->data[i]);
        
        tty_reshow();
        
        entry = (struct klog_entry*)((char*)entry + (entry->length + sizeof(struct klog_entry)));
    }

    fb_clear();
    tty_reshow_full();
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
        klog_logln(0, FATAL, message_string, args);
    }
    else
    {
        klog_early_logln(FATAL, "Exception occured in kernel:");
        klog_early_logln(FATAL, message_string, args);
    }

    reshow_log();
    halt();
}

void __attribute__((noreturn)) kvpanic_intr(struct intr_stack *stack, const char* message_string, va_list args)
{
    if(klog_is_init())
    {
        klog_logln(0, FATAL, "Exception occured in kernel:");
        klog_logln(0, FATAL, message_string, args);
    }
    else
    {
        klog_early_logln(FATAL, "Exception occured in kernel:");
        klog_early_logln(FATAL, message_string, args);
    }
    dump_registers(stack);

    reshow_log();
    halt();
}
