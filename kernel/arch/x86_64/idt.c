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
#include <common/util/kfuncs.h>
#include <common/mm/mm.h>
#include <common/tty/fb.h>
#include <common/tty/tty.h>

#include <arch/idt.h>
#include <stack_state.h>

#define IDT_TYPE_INTERRUPT    0xE
#define IDT_TYPE_TRAP         0xF

extern void isr0_entry(); // Divide By 0
extern void isr1_entry(); // Debug
extern void isr2_entry(); // NMI
extern void isr3_entry(); // Breakpoint
extern void isr4_entry(); // Overflow
extern void isr5_entry(); // Bound Range
extern void isr6_entry(); // Invalid Opcode
extern void isr7_entry(); // No Coprocessor
extern void isr8_entry(); // Double Fault
// Reserved 9 (Coprocessor Segment Overrun)
extern void isr10_entry(); // Invalid TSS
extern void isr11_entry(); // Segment Not Present
extern void isr12_entry(); // Stack Fault
extern void isr13_entry(); // General Protection Fault
extern void isr14_entry(); // Page Fault
// Reserved 15
extern void isr16_entry(); // Floating Point Exception
extern void isr17_entry(); // Alignment Check
extern void isr18_entry(); // Machine Check
extern void isr19_entry(); // SIMD FP Exception
extern void isr20_entry(); // Virtualization Exception (AMD64 Reserved)
// Reserved 21-28
extern void isr29_entry(); // Virtualization Exception (Intel Reserved)
extern void isr30_entry(); // Security Exception (Intel Reserved)

extern void irq0_entry();
extern void irq1_entry();
extern void irq2_entry();
extern void irq3_entry();
extern void irq4_entry();
extern void irq5_entry();
extern void irq6_entry();
extern void irq7_entry();
extern void irq8_entry();
extern void irq9_entry();
extern void irq10_entry();
extern void irq11_entry();
extern void irq12_entry();
extern void irq13_entry();
extern void irq14_entry();
extern void irq15_entry();

// IO APIC Stuff
extern void irq16_entry();
extern void irq17_entry();
extern void irq18_entry();
extern void irq19_entry();
extern void irq20_entry();
extern void irq21_entry();
extern void irq22_entry();
extern void irq23_entry();

// Only for definite spurious irqs
extern void spurious_irq();

extern void syscall_entry();

typedef struct
{
    uint16_t offset_low0;
    uint16_t selector;
    uint8_t ist : 3;
    uint8_t rsv0 : 5;
    uint8_t type : 4;
    uint8_t zero : 1;
    uint8_t dpl : 2;
    uint8_t present : 1;
    uint16_t offset_low1;
    uint32_t offset_high;
    uint32_t rsv1;
} __attribute__((__packed__)) idt_descriptor_t;
extern idt_descriptor_t idt_table;

const char* fault_names[] = {
    "(#DE) Division by 0",
    "(#DB) Debug Exception",
    "NMI Exception",
    "(#BP) Breakpoint",
    "(#OE) Overflow",
    "(#BR) Bound Range Exceeded",
    "(#UD) Invalid Opcode",
    "(#NM) No Coprocessor",
    "(#DF) Double Fault",
    "(CSO) Reserved Fault",
    "(#TS) Invalid TSS",
    "(#NP) Segment Not Present",
    "(#SS) Stack Segment Fault",
    "(#GP) General Protection Fault",
    "(#PF) Page Fault",
    "(RSV) Reserved Fault",
    "(#MF) Floating Point Exception",
    "(#AC) Alignment Check Exception",
    "(#MC) Machine Check Exception",
    "(#XM) SIMD/FP Exception",
    "(#VE) VMX Virtualization Exception", // AMD Reserved
    "(#CE) Control Protection Exception", // AMD Reserved
    "Reserved Fault",
    "Reserved Fault",
    "Reserved Fault",
    "Reserved Fault",
    "Reserved Fault",
    "Reserved Fault",
    "Reserved Fault",
    "(#VE) SVM Virtualization Exception", // Intel Reserved
    "(#SE) SVM Security Exception",       // Intel Reserved
    "Reserved Fault",
};
struct isr_handler function_table[256];

static void create_descriptor(uint8_t index,
                              uint64_t base,
                              uint16_t selector,
                              uint8_t type,
                              uint8_t ist)
{
    (&idt_table)[index].offset_low0 = (base >>  0) & 0x0000FFFF;
    (&idt_table)[index].offset_low1 = (base >> 16) & 0x0000FFFF;
    (&idt_table)[index].offset_high = (base >> 32) & 0xFFFFFFFF;
    (&idt_table)[index].ist = ist & 0x7;
    (&idt_table)[index].selector = selector & ~0x7;
    (&idt_table)[index].type = type;
    (&idt_table)[index].dpl = selector & 0x3;
    (&idt_table)[index].zero = 0;
    (&idt_table)[index].present = 1;
}

static void default_exception(struct intr_stack *frame, void* params)
{
    kpanic_intr(frame, fault_names[frame->int_num]);
}

void isr_common(struct intr_stack *frame)
{
    if(function_table[frame->int_num].function != KNULL)
    {
        isr_t function = function_table[frame->int_num].function;
        // Bit of a workaround to merge the parameters and stack frame
        if(frame->int_num < 32)
            function(frame, frame->int_num);
        else
            function(function_table[frame->int_num].parameters, frame->int_num);
    }
}

void isr_add_handler(uint8_t index, isr_t function, void* parameters)
{
    function_table[index].function = function;
    function_table[index].parameters = parameters;
}

void setup_idt()
{
    // Generic exceptions
    for(int i = 0; i < 32; i++)
    {
        function_table[i].function = default_exception;
        function_table[i].parameters = NULL;
    }

    // The rest of the handlers
    for(int i = 32; i < 256; i++)
    {
        function_table[i].function = KNULL;
        function_table[i].parameters = NULL;
    }

    create_descriptor( 0, (uint64_t) isr0_entry, 0x08, IDT_TYPE_INTERRUPT, 0);
    create_descriptor( 1, (uint64_t) isr1_entry, 0x08, IDT_TYPE_INTERRUPT, 0);
    create_descriptor( 2, (uint64_t) isr2_entry, 0x08, IDT_TYPE_INTERRUPT, 0);
    create_descriptor( 3, (uint64_t) isr3_entry, 0x08, IDT_TYPE_INTERRUPT, 0);
    create_descriptor( 4, (uint64_t) isr4_entry, 0x08, IDT_TYPE_INTERRUPT, 0);
    create_descriptor( 5, (uint64_t) isr5_entry, 0x08, IDT_TYPE_INTERRUPT, 0);
    create_descriptor( 6, (uint64_t) isr6_entry, 0x08, IDT_TYPE_INTERRUPT, 0);
    create_descriptor( 7, (uint64_t) isr7_entry, 0x08, IDT_TYPE_INTERRUPT, 0);
    // #DF uses a separate stack to handle the exception
    create_descriptor( 8, (uint64_t) isr8_entry, 0x08, IDT_TYPE_INTERRUPT, 1);
    create_descriptor(10, (uint64_t)isr10_entry, 0x08, IDT_TYPE_INTERRUPT, 0);
    create_descriptor(11, (uint64_t)isr11_entry, 0x08, IDT_TYPE_INTERRUPT, 0);
    // #SF uses a separate stack to handle the exception
    create_descriptor(12, (uint64_t)isr12_entry, 0x08, IDT_TYPE_INTERRUPT, 1);
    create_descriptor(13, (uint64_t)isr13_entry, 0x08, IDT_TYPE_INTERRUPT, 0);
    // #PF uses a separate stack to handle the exception
    create_descriptor(14, (uint64_t)isr14_entry, 0x08, IDT_TYPE_INTERRUPT, 1);
    // RSV 15
    create_descriptor(16, (uint64_t)isr16_entry, 0x08, IDT_TYPE_INTERRUPT, 0);
    create_descriptor(17, (uint64_t)isr17_entry, 0x08, IDT_TYPE_INTERRUPT, 0);
    create_descriptor(18, (uint64_t)isr18_entry, 0x08, IDT_TYPE_INTERRUPT, 0);
    create_descriptor(19, (uint64_t)isr19_entry, 0x08, IDT_TYPE_INTERRUPT, 0);
    // RSV 20-28
    create_descriptor(29, (uint64_t)isr29_entry, 0x08, IDT_TYPE_INTERRUPT, 0);
    create_descriptor(30, (uint64_t)isr30_entry, 0x08, IDT_TYPE_INTERRUPT, 0);
    // RSV 31
    // IRQs
    // Master PIC
    create_descriptor(32, (uint64_t) irq0_entry, 0x08, IDT_TYPE_INTERRUPT, 0);
    create_descriptor(33, (uint64_t) irq1_entry, 0x08, IDT_TYPE_INTERRUPT, 0);
    create_descriptor(34, (uint64_t) irq2_entry, 0x08, IDT_TYPE_INTERRUPT, 0);
    create_descriptor(35, (uint64_t) irq3_entry, 0x08, IDT_TYPE_INTERRUPT, 0);
    create_descriptor(36, (uint64_t) irq4_entry, 0x08, IDT_TYPE_INTERRUPT, 0);
    create_descriptor(37, (uint64_t) irq5_entry, 0x08, IDT_TYPE_INTERRUPT, 0);
    create_descriptor(38, (uint64_t) irq6_entry, 0x08, IDT_TYPE_INTERRUPT, 0);
    create_descriptor(39, (uint64_t) irq7_entry, 0x08, IDT_TYPE_INTERRUPT, 0);
    // Slave PIC
    create_descriptor(40, (uint64_t) irq8_entry, 0x08, IDT_TYPE_INTERRUPT, 0);
    create_descriptor(41, (uint64_t) irq9_entry, 0x08, IDT_TYPE_INTERRUPT, 0);
    create_descriptor(42, (uint64_t)irq10_entry, 0x08, IDT_TYPE_INTERRUPT, 0);
    create_descriptor(43, (uint64_t)irq11_entry, 0x08, IDT_TYPE_INTERRUPT, 0);
    create_descriptor(44, (uint64_t)irq12_entry, 0x08, IDT_TYPE_INTERRUPT, 0);
    create_descriptor(45, (uint64_t)irq13_entry, 0x08, IDT_TYPE_INTERRUPT, 0);
    create_descriptor(46, (uint64_t)irq14_entry, 0x08, IDT_TYPE_INTERRUPT, 0);
    create_descriptor(47, (uint64_t)irq15_entry, 0x08, IDT_TYPE_INTERRUPT, 0);
    // Extra IO APIC entries
    create_descriptor(48, (uint64_t)irq16_entry, 0x08, IDT_TYPE_INTERRUPT, 0);
    create_descriptor(49, (uint64_t)irq17_entry, 0x08, IDT_TYPE_INTERRUPT, 0);
    create_descriptor(50, (uint64_t)irq18_entry, 0x08, IDT_TYPE_INTERRUPT, 0);
    create_descriptor(51, (uint64_t)irq19_entry, 0x08, IDT_TYPE_INTERRUPT, 0);
    create_descriptor(52, (uint64_t)irq20_entry, 0x08, IDT_TYPE_INTERRUPT, 0);
    create_descriptor(53, (uint64_t)irq21_entry, 0x08, IDT_TYPE_INTERRUPT, 0);
    create_descriptor(54, (uint64_t)irq22_entry, 0x08, IDT_TYPE_INTERRUPT, 0);
    create_descriptor(55, (uint64_t)irq23_entry, 0x08, IDT_TYPE_INTERRUPT, 0);

    // Interrupt syscall
    create_descriptor(0x80, (uint64_t)syscall_entry, 0x0B, IDT_TYPE_INTERRUPT, 0);

    // Spurious IRQ entries
    for(size_t i = 0; i < 16; i++)
        create_descriptor(i + 0xF0, (uint64_t)spurious_irq, 0x08, IDT_TYPE_TRAP, 0);
}
