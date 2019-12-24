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
#include <common/hal.h>

#include <stdio.h>

#include <common/acpi.h>
#include <common/hal/timer.h>
#include <common/mm/liballoc.h>
#include <common/sched/sched.h>
#include <common/util/kfuncs.h>

#include <arch/msr.h>
#include <arch/apic.h>
#include <arch/pic.h>
#include <arch/pit.h>
#include <arch/idt.h>
#include <arch/io.h>
#include <arch/iobase.h>
#include <stack_state.h>

//TODO: Breakup HAL into more subsystems

#define KLOG_FATAL(msg, ...) klog_logln(LVL_FATAL, msg, __VA_ARGS__);

extern void halt();

static bool use_apic = false;
static size_t native_flags = 0;
static struct heap_info heap_context = {
#if defined(__x86_64__)
    .base=0xFFFF900000000000, .length=0x0000100000000000
#else
    .base=0xD0000000, .length=0x10000000
#endif
};

static struct ic_dev* default_ic;
static uint8_t ic_mode = IC_MODE_PIC;

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
    bool force_pic = false;
    ACPI_TABLE_MADT* madt = (ACPI_TABLE_MADT*)acpi_early_get_table(ACPI_SIG_MADT, 1);

    if(!force_pic && madt != NULL)
    {
        // We have a MADT, and most likely an APIC/IOAPIC

        // If there is a legacy PIC, disable it
        if((madt->Flags & ACPI_MADT_PCAT_COMPAT) == ACPI_MADT_DUAL_PIC)
            pic_get_dev()->disable(0xF0);

        uint64_t apic_base = (uint64_t)madt->Address;

        // Common
        uint32_t current_table_instance = 1;

        // Initialize BSP LAPIC
        ACPI_MADT_LOCAL_APIC* madt_lapic = (ACPI_MADT_LOCAL_APIC*)madt_find_table(madt, ACPI_MADT_TYPE_LOCAL_APIC, 1);
        if(madt_lapic == NULL || (madt_lapic->LapicFlags & ACPI_MADT_ENABLED) != ACPI_MADT_ENABLED)
        {
            klog_logln(LVL_ERROR, "No APIC found");
            return;
        }
        else
        {
            apic_init(apic_base);
        }

        // Initialize IOAPICs
        current_table_instance = 1;
        ACPI_MADT_IO_APIC* madt_ioapic = (ACPI_MADT_IO_APIC*)madt_find_table(madt, ACPI_MADT_TYPE_IO_APIC, 1);
        while(madt_ioapic != NULL)
        {
            ioapic_init(madt_ioapic->Address, madt_ioapic->GlobalIrqBase);
            madt_ioapic = (ACPI_MADT_IO_APIC*)madt_find_table(madt, ACPI_MADT_TYPE_IO_APIC, ++current_table_instance);
        }

        // Setup IOAPIC routing
        current_table_instance = 1;
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
            madt_iso = (ACPI_MADT_INTERRUPT_OVERRIDE*)madt_find_table(madt, ACPI_MADT_TYPE_INTERRUPT_OVERRIDE, ++current_table_instance);
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
    acpi_put_table((ACPI_TABLE_HEADER*)madt);
    // acpi_set_pic_mode(ic_mode);
    
    timer_init();
    pit_init();
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

void* get_klog_base()
{
    return (void*)KLOG_BASE;
}

void arch_reboot()
{
    // Pulse the reset line
    outb(0x64, 0xFE);

    halt();
}

void dump_registers(struct intr_stack *stack)
{
#if defined(__x86_64__)
    KLOG_FATAL("%s", "***BEGIN REGISTER DUMP***");
    KLOG_FATAL("%s", "RAX RBX RCX RDX");
    KLOG_FATAL("%#p %#p %#p %#p", stack->rax, stack->rbx, stack->rcx, stack->rdx);
    KLOG_FATAL("%s", "R8  R9  R10 R11");
    KLOG_FATAL("%#p %#p %#p %#p", stack->r8, stack->r9, stack->r10, stack->r11);
    KLOG_FATAL("%s", "R12 R13 R14 R15");
    KLOG_FATAL("%#p %#p %#p %#p\n", stack->r12, stack->r13, stack->r14, stack->r15);
    KLOG_FATAL("%s", "RSI RDI RSP RBP");
    KLOG_FATAL("%#p %#p %#p %#p", stack->rsi, stack->rdi, stack->rsp, stack->rbp);
    KLOG_FATAL("CR2: %#p", stack->cr2);
    KLOG_FATAL("RIP: %#p", stack->rip);
    KLOG_FATAL("Error code: %x", stack->err_code);
    KLOG_FATAL("CS: %x", stack->cs);
    KLOG_FATAL("SS: %x", stack->ss);
    KLOG_FATAL("RFLAGS: %x", stack->rflags);

#elif defined(__i386__)
    KLOG_FATAL("%s", "***BEGIN REGISTER DUMP***");
    KLOG_FATAL("EAX: %#p, EBX: %#p, ECX: %#p, EDX: %#p", stack->eax, stack->ebx, stack->ecx, stack->edx);
    KLOG_FATAL("ESI: %#p, EDI: %#p, ESP: %#p, EBP: %#p", stack->esi, stack->edi, stack->esp, stack->ebp);
    KLOG_FATAL("EIP: %#p", stack->eip);
    KLOG_FATAL("Error code: %x", stack->err_code);
    KLOG_FATAL("CR2: %p", stack->cr2);

#endif

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
