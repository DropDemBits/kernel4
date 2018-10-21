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
#include <common/sched/sched.h>
#include <common/util/kfuncs.h>

#include <arch/pic.h>
#include <arch/pit.h>
#include <arch/idt.h>
#include <stack_state.h>

#define MAX_TIMERS 64

#define KLOG_FATAL(msg, ...) \
    if(klog_is_init()) {klog_logln(0, FATAL, msg, __VA_ARGS__);} \
    else {klog_early_logln(FATAL, msg, __VA_ARGS__);}

static bool use_apic = false;
static size_t native_flags = 0;
static struct heap_info heap_context = {
#if defined(__x86_64__)
    .base=0xFFFF900000000000, .length=0x0000100000000000
#else
    .base=0x90000000, .length=0x10000000
#endif
};

// ID 0 is reserved (1 = free, 0 = set)
static uint64_t timer_id_bitmap = ~1;
// Default timer (index to array below)
static unsigned int default_timer = 0x0;
static struct timer_dev* timers[MAX_TIMERS];

// Valid timer id's are in the range of 1 - 64
static unsigned int next_timer_id()
{
    for(unsigned int i = 0; i < 64; i++)
    {
        if((timer_id_bitmap & (1ULL << i)) == 0)
            continue;
        
        timer_id_bitmap &= ~(1ULL << i);
        return i+1;
    }

    return 0;
}

static bool apic_exists()
{
    uint32_t edx;
    asm volatile("cpuid":"=d"(edx):"a"(1));
    return (edx >> 9) & 0x1;
}

void hal_init()
{
    // Check for APIC
    if(apic_exists())
    {
        // TODO: Enable & init apic
        //outb(PIC1_DATA, 0xFF);
        //outb(PIC2_DATA, 0xFF);
    }
    pic_get_dev()->enable(0x20);

    if(!use_apic)
    {
        for(int i = 0; i < 16; i++) pic_eoi(i);

        // Mask all IRQs
        for(int i = 0; i < 16; i++)
            pic_mask(i);
    }

    for(int i = 0; i < MAX_TIMERS; i++)
        timers[i] = KNULL;
    
    pit_init();
}

unsigned long timer_add(struct timer_dev* device, enum timer_type type)
{
    if(device == KNULL)
        return 0;
    
    device->id = next_timer_id();
    device->list_head = KNULL;

    if(!device->id)
        return 0;
    
    timers[device->id - 1] = device;

    return device->id;
}

void timer_set_default(unsigned long id)
{
    if(id == 0)
    {
        // ID 0 is reserved for default timer
        return;
    }

    default_timer = id - 1;
}

void timer_add_handler(unsigned long id, timer_handler_t handler_function)
{
    // Timer id is an index to the array, not the actual id itself
    unsigned long timer_id = id - 1;
    if(id > MAX_TIMERS)
        return;
    else if(id == 0)
        timer_id = default_timer;
    else if(timers[timer_id] == KNULL)
        return;
    
    struct timer_handler_node* node = kmalloc(sizeof(struct timer_handler_node));
    if(node == NULL)
        return;
    
    node->handler = handler_function;
    node->next = KNULL;

    if(timers[timer_id]->list_head == KNULL)
    {
        // The list is empty, so this node begins the new list
        timers[timer_id]->list_head = node;
        return;
    }

    // We'll have to go through the entire loop to append the node
    struct timer_handler_node* current_node = KNULL;
    struct timer_handler_node* next_node = timers[timer_id]->list_head;

    while(next_node != KNULL)
    {
        current_node = next_node;
        next_node = next_node->next;
    }

    if(current_node == KNULL)
    {
        kfree(node);
        return;
    }

    current_node->next = node;
}

void timer_broadcast_update(unsigned long id)
{
    // Timer id is an index to the array, not the actual id itself
    unsigned int timer_id = id - 1;
    if(id > MAX_TIMERS)
        return;
    else if(id == 0)
        timer_id = default_timer;
    if(timers[timer_id] == KNULL)
        return;

    if(timers[timer_id]->list_head == KNULL)  // No point in going through the loop
        return;

    struct timer_handler_node* node = timers[timer_id]->list_head;

    while(node != KNULL)
    {
        node->handler(timers[timer_id]);
        node = node->next;
    }
}

uint64_t timer_read_counter(unsigned long id)
{
    unsigned int timer_id = id - 1;
    if(id > MAX_TIMERS)
        return 0;
    else if(id == 0)
        timer_id = default_timer;
    if(timers[timer_id] == KNULL)
        return 0;
    
    return timers[timer_id]->counter;
}

void ic_mask(uint16_t irq)
{
    hal_get_ic()->mask(irq);
}

void ic_unmask(uint16_t irq)
{
    hal_get_ic()->unmask(irq);
}

bool ic_check_spurious(uint16_t irq)
{
    return hal_get_ic()->is_spurious(irq);
}

struct irq_handler* ic_irq_handle(uint8_t irq, enum irq_type type, irq_function_t handler)
{
    return hal_get_ic()->handle_irq(irq, handler);
}

void ic_irq_free(struct irq_handler* handler)
{
    return;
}

struct ic_dev* hal_get_ic()
{
    return pic_get_dev();
}

struct heap_info* get_heap_info()
{
    return &heap_context;
}

#if defined(__x86_64__)
uint64_t get_klog_base()
{
    return 0xFFFF8C0000000000;
}

uint64_t get_driver_mmio_base()
{
    return 0xFFFFF00000000000;
}

void dump_registers(struct intr_stack *stack)
{
    KLOG_FATAL("%s", "***BEGIN REGISTER DUMP***");
    KLOG_FATAL("%s", "RAX RBX RCX RDX");
    KLOG_FATAL("%#p %#p %#p %#p", stack->rax, stack->rbx, stack->rcx, stack->rdx);
    KLOG_FATAL("%s", "RSI RDI RSP RBP");
    KLOG_FATAL("%#p %#p %#p %#p", stack->rsi, stack->rdi, stack->rsp, stack->rbp);
    KLOG_FATAL("RIP: %#p", stack->rip);
    KLOG_FATAL("Error code: %x", stack->err_code);

    thread_t* at = sched_active_thread();
    KLOG_FATAL("Current Thread: %#p", at);
    if(((uintptr_t)at & ~0xFFF) != KNULL && ((uintptr_t)at & ~0xFFF) != NULL)
    {
        KLOG_FATAL("\tID: %d (%s)", at->tid, at->name);
        KLOG_FATAL("\tPriority: %d", at->priority);
        KLOG_FATAL("\tKSP: %#p, SP: %#p", at->kernel_sp, at->user_sp);
    } else
    {
        KLOG_FATAL("%s", "\t(Pre-scheduler)");
    }
}
#elif defined(__i386__)
uint64_t get_klog_base()
{
    return 0x8C000000;
}

uint64_t get_driver_mmio_base()
{
    return 0xF0000000;
}

void dump_registers(struct intr_stack *stack)
{
    KLOG_FATAL("%s", "***BEGIN REGISTER DUMP***");
    KLOG_FATAL("EAX: %#p, EBX: %#p, ECX: %#p, EDX: %#p", stack->eax, stack->ebx, stack->ecx, stack->edx);
    KLOG_FATAL("ESI: %#p, EDI: %#p, ESP: %#p, EBP: %#p", stack->esi, stack->edi, stack->esp, stack->ebp);
    KLOG_FATAL("EIP: %#p", stack->eip);
    KLOG_FATAL("Error code: %x", stack->err_code);
    KLOG_FATAL("CR2: %p", stack->cr2);
    thread_t* at = sched_active_thread();
    KLOG_FATAL("Current Thread: %#p", at);
    if(at != KNULL)
    {
        KLOG_FATAL("\tID: %d (%s)", at->tid, at->name);
        KLOG_FATAL("\tPriority: %d", at->priority);
        KLOG_FATAL("\tKSP: %#p, SP: %#p", at->kernel_sp, at->user_sp);
    } else
    {
        KLOG_FATAL("%s", "\t(Pre-scheduler)");
    }
}
#endif
