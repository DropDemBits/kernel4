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

#include <common/sched.h>
#include <common/hal.h>
#include <arch/pic.h>
#include <arch/pit.h>
#include <arch/idt.h>
#include <x86_64/stack_state.h>

#define MAX_TIMERS 64

extern void irq_common(struct intr_stack *frame);

static bool use_apic = false;
static size_t native_flags = 0;
static struct heap_info heap_context = {
    .base=0xFFFF900000000000, .length=0x0000100000000000
};

// ID 0 is reserved
static uint64_t timer_id_bitmap = 0;
// Default timer (index to array below)
static unsigned int default_timer = 0x0;
static struct timer_dev* timers[MAX_TIMERS];

// Valid timer id's are in the range of 1 - 64
static unsigned int next_timer_id()
{
    for(unsigned int i = 0; i < 64; i++)
    {
        if(timer_id_bitmap & (1ULL << i) == 0)
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
    pic_init();

    for(int i = 0; i < 16; i++)
        irq_add_handler(i, (isr_t)irq_common);

    if(!use_apic)
    {
        for(int i = 0; i < 16; i++) pic_eoi(i);
    }

    for(int i = 0; i < MAX_TIMERS; i++)
        timers[i] = KNULL;
    
    pit_init();

    pic_unmask(0);
    pic_mask(8);

}

void hal_enable_interrupts()
{
    asm volatile("sti");
}

void hal_disable_interrupts()
{
    asm volatile("cli");
}

void hal_save_interrupts()
{
    asm volatile("pushfq\n\tpopq %%rax":"=a"(native_flags));
    hal_disable_interrupts();
}

void hal_restore_interrupts()
{
    asm volatile("push %%rax\n\tpopfq"::"a"(native_flags));
}

inline void busy_wait()
{
    asm volatile("pause");
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
    default_timer = id - 1;
}

void timer_add_handler(unsigned long id, timer_handler_t handler_function)
{
    // Timer id is an index to the array, not the actual id itself
    unsigned int timer_id = id - 1;
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
    struct timer_handler_node* current_node;
    struct timer_handler_node* next_node = timers[timer_id]->list_head;

    while(next_node != KNULL)
    {
        current_node = next_node;
        next_node = next_node->next;
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
        return;
    else if(id == 0)
        timer_id = default_timer;
    if(timers[timer_id] == KNULL)
        return;
    
    return timers[timer_id]->counter;
}

void timer_config_counter(uint16_t id, uint16_t frequency, uint8_t mode)
{
    pit_init_counter(id, frequency, mode);
}

void timer_reset_counter(uint16_t id)
{
    pit_reset_counter(id);
}

void timer_set_counter(uint16_t id, uint16_t frequency)
{
    pit_set_counter(id, frequency);
}

void ic_mask(uint16_t irq)
{
    pic_mask(irq);
}

void ic_unmask(uint16_t irq)
{
    pic_unmask(irq);
}

void ic_check_spurious(uint16_t irq)
{
    pic_check_spurious(irq);
}

void ic_eoi(uint16_t irq)
{
    pic_eoi(irq);
}

void irq_add_handler(uint16_t irq, isr_t handler)
{
    isr_add_handler(irq + 32, handler);
}

struct heap_info* get_heap_info()
{
    return &heap_context;
}

void dump_registers(struct intr_stack *stack)
{
    printf("***BEGIN REGISTER DUMP***\n");
    puts("RAX RBX RCX RDX");
    printf("%#p %#p %#p %#p\n", stack->rax, stack->rbx, stack->rcx, stack->rdx);
    puts("RSI RDI RSP RBP");
    printf("%#p %#p %#p %#p\n", stack->rsi, stack->rdi, stack->rsp, stack->rbp);
    printf("RIP: %#p\n", stack->rip);
    printf("Error code: %x\n", stack->err_code);
    thread_t* at = sched_active_thread();
    printf("Current Thread: %#p\n", at);
    if(at != KNULL)
    {
        printf("\tID: %d (%s)\n", at->tid, at->name);
        printf("\tPriority: %d\n", at->priority);
        printf("\tKRSP: %#p, RSP: %#p", at->kernel_sp, at->user_sp);
    } else
    {
        puts("(Pre-scheduler)");
    }
}

void intr_wait()
{
    asm("hlt");
}
