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
#include <common/acpi.h>
#include <common/sched/sched.h>
#include <common/util/kfuncs.h>

#include <arch/msr.h>
#include <arch/apic.h>
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

static struct ic_dev* default_ic;
static uint8_t ic_mode = IC_MODE_PIC;

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

static ACPI_SUBTABLE_HEADER* madt_find_table(ACPI_TABLE_MADT* madt, uint8_t Type, uint32_t Instance)
{
    if(Instance == 0)
        return NULL;

    ACPI_SUBTABLE_HEADER* madt_ics = (ACPI_SUBTABLE_HEADER*)(madt + 1);
    uint32_t len = madt->Header.Length - sizeof(ACPI_TABLE_MADT);

    while(len > 0)
    {
        if(madt_ics->Type == Type)
        {
            Instance--;
            if(Instance == 0)
                return madt_ics;
        }
        len -= madt_ics->Length;
        madt_ics = (ACPI_SUBTABLE_HEADER*)((uintptr_t)madt_ics + madt_ics->Length);
    }

    return NULL;
}

void hal_init()
{
    ACPI_TABLE_MADT* madt = (ACPI_TABLE_MADT*)acpi_early_get_table(ACPI_SIG_MADT, 1);

    if(madt != NULL)
    {
        // We have a MADT

        // If there is a legacy PIC, disable it
        if((madt->Flags & ACPI_MADT_PCAT_COMPAT) == ACPI_MADT_DUAL_PIC)
            pic_get_dev()->disable(0xF0);

        uint64_t apic_base = (uint64_t)madt->Address;

        // Common
        uint32_t current_instance = 1;

        // Initialize BSP LAPIC
        ACPI_MADT_LOCAL_APIC* madt_lapic = (ACPI_MADT_LOCAL_APIC*)madt_find_table(madt, ACPI_MADT_TYPE_LOCAL_APIC, 1);
        if(madt_lapic == NULL || (madt_lapic->LapicFlags & ACPI_MADT_ENABLED) != ACPI_MADT_ENABLED)
        {
            klog_early_logln(ERROR, "No APIC found");
            return;
        }
        else
        {
            apic_init(apic_base);
        }

        // Initialize IOAPICs
        current_instance = 1;
        ACPI_MADT_IO_APIC* madt_ioapic = (ACPI_MADT_IO_APIC*)madt_find_table(madt, ACPI_MADT_TYPE_IO_APIC, 1);
        while(madt_ioapic != NULL)
        {
            ioapic_init(madt_ioapic->Address, madt_ioapic->GlobalIrqBase);
            madt_ioapic = (ACPI_MADT_IO_APIC*)madt_find_table(madt, ACPI_MADT_TYPE_IO_APIC, ++current_instance);
        }

        // Setup IOAPIC routing
        current_instance = 1;
        ACPI_MADT_INTERRUPT_OVERRIDE* madt_iso = (ACPI_MADT_INTERRUPT_OVERRIDE*)madt_find_table(madt, ACPI_MADT_TYPE_INTERRUPT_OVERRIDE, 1);
        while(madt_iso != NULL)
        {
            // Skip non-ISA routings
            if(madt_iso->Bus != 0)
                goto skip_entry;

            uint8_t polarity = (uint8_t)((madt_iso->IntiFlags) & 0x3);
            uint8_t trigger_mode = (uint8_t)((madt_iso->IntiFlags >> 2) & 0x3);

            // If the polarity is bus-conformant, set to active low
            if(polarity == ACPI_MADT_POLARITY_CONFORMS)
                polarity = IOAPIC_POLARITY_HIGH;
            else 
                polarity >>= 1;

            // If the trigger mode is bus-conformant, set to edge triggered
            if(trigger_mode == ACPI_MADT_TRIGGER_CONFORMS)
                trigger_mode = IOAPIC_TRIGGER_EDGE;
            else
                trigger_mode >>= 1;

            ioapic_route_line(madt_iso->GlobalIrq, madt_iso->SourceIrq, polarity, trigger_mode);

        skip_entry:
            madt_iso = (ACPI_MADT_INTERRUPT_OVERRIDE*)madt_find_table(madt, ACPI_MADT_TYPE_INTERRUPT_OVERRIDE, ++current_instance);
        }

        ACPI_MADT_LOCAL_APIC_NMI* madt_nmi = (ACPI_MADT_LOCAL_APIC_NMI*)madt_find_table(madt, ACPI_MADT_TYPE_LOCAL_APIC_NMI, 1);
        if(madt_nmi != NULL)
            apic_set_lint_entry(madt_nmi->Lint, (madt_nmi->IntiFlags >> 1) & 1, (madt_nmi->IntiFlags >> 3) & 1, APIC_DELMODE_NMI);
        
        default_ic = ioapic_get_dev();
        ic_mode = IC_MODE_IOAPIC;
    }
    /*else if(apic_exists())
    {
        // We may not have ACPI, but have an APIC (ie. have an MP table) 
        // Disable the legacy PIC
        pic_get_dev()->disable(0xF0);

        // Read the MSR for the APIC base
        uint64_t apic_base = msr_read(MSR_IA32_APIC_BASE) & ~0xFFF;
        apic_init(apic_base);

        default_ic = ioapic_get_dev();
        ic_mode = IC_MODE_IOAPIC;
    }*/
    else
    {
        // The system only has a legacy PIC, init that
        pic_get_dev()->enable(0x20);

        // Mask all IRQs
        for(int i = 0; i < 16; i++)
            pic_get_dev()->mask(i);

        default_ic = pic_get_dev();
        ic_mode = IC_MODE_PIC;
    }
    acpi_put_table(madt);
    // acpi_set_pic_mode(ic_mode);

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
    if(timers[timer_id] == KNULL || timers[timer_id] == NULL)
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

struct irq_handler* ic_irq_handle(uint8_t irq, uint32_t flags, irq_function_t handler)
{
    if(ic_mode == IC_MODE_PIC)
    {
        return hal_get_ic()->handle_irq(irq, flags, handler);
    }
    else
    {
        uint8_t real_irqn = irq;

        if((flags & INT_SRC_MASK) == INT_SRC_LEGACY)
            real_irqn = ioapic_map_irq(irq);

        return hal_get_ic()->handle_irq(real_irqn, flags, handler);
    }
}

void ic_irq_free(struct irq_handler* handler)
{
    hal_get_ic()->free_irq(handler);
}

struct ic_dev* hal_get_ic()
{
    return default_ic;
}

uint8_t hal_get_ic_mode()
{
    return ic_mode;
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
    KLOG_FATAL("CR2: %#p", stack->cr2);
    KLOG_FATAL("RIP: %#p", stack->rip);
    KLOG_FATAL("Error code: %x", stack->err_code);

    thread_t* at = sched_active_thread();
    KLOG_FATAL("Current Thread: %#p", at);
    if(((uintptr_t)at & ~0xFFF) != (uintptr_t)KNULL && ((uintptr_t)at & ~0xFFF) != (uintptr_t)NULL)
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
