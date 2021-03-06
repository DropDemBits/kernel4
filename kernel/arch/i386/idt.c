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

#include <arch/idt.h>
#include <stack_state.h>

#define IDT_TYPE_TASK            0x5
#define IDT_TYPE_INTERRUPT16    0x6
#define IDT_TYPE_TRAP16         0x7
#define IDT_TYPE_INTERRUPT        0xE
#define IDT_TYPE_TRAP             0xF

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
    uint16_t offset_low;
    uint16_t selector;
    uint8_t rsv;
    uint8_t type : 4;
    uint8_t zero : 1;
    uint8_t dpl : 2;
    uint8_t present : 1;
    uint16_t offset_high;
} __attribute__((__packed__)) idt_descriptor_t;
extern idt_descriptor_t idt_table;

const char* fault_names[] = {
    "Division by 0",
    "Debug Exception",
    "NMI Exception",
    "Breakpoint",
    "Overflow Flag Set",
    "Bound Range Exceeded",
    "Invalid Opcode",
    "No Coprocessor",
    "Double Fault",
    "Reserved Fault",
    "Invalid TSS",
    "Segment Not Present",
    "Stack Fault",
    "General Protection Fault",
    "Page Fault",
    "Reserved Fault",
    "Floating Point Exception",
    "Alignment Check Exception",
    "Machine Check Exception",
    "SIMD/FP Exception",
    "VMX Virtualization Exception", // AMD Reserved
    "Control Protection Exception", // AMD Reserved
    "Reserved Fault",
    "Reserved Fault",
    "Reserved Fault",
    "Reserved Fault",
    "Reserved Fault",
    "Reserved Fault",
    "Reserved Fault",
    "SVM Virtualization Exception", // Intel Reserved
    "SVM Security Exception",       // Intel Reserved
    "Reserved Fault",
};
struct isr_handler function_table[256];

static void create_descriptor(uint8_t index,
                              uint32_t base,
                              uint16_t selector,
                              uint8_t type)
{
    (&idt_table)[index].offset_low =  (base >> 0) & 0xFFFF;
    (&idt_table)[index].offset_high = (base >> 16) & 0xFFFF;
    (&idt_table)[index].selector = selector & ~0x7;
    (&idt_table)[index].type = type;
    (&idt_table)[index].dpl = selector & 0x3;
    (&idt_table)[index].zero = 0;
    (&idt_table)[index].rsv = 0;
    (&idt_table)[index].present = 1;
}

static void default_exception(struct intr_stack *frame)
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

    create_descriptor( 0, (uint32_t) isr0_entry, 0x08, IDT_TYPE_INTERRUPT);
    create_descriptor( 1, (uint32_t) isr1_entry, 0x08, IDT_TYPE_INTERRUPT);
    create_descriptor( 2, (uint32_t) isr2_entry, 0x08, IDT_TYPE_INTERRUPT);
    create_descriptor( 3, (uint32_t) isr3_entry, 0x08, IDT_TYPE_INTERRUPT);
    create_descriptor( 4, (uint32_t) isr4_entry, 0x08, IDT_TYPE_INTERRUPT);
    create_descriptor( 5, (uint32_t) isr5_entry, 0x08, IDT_TYPE_INTERRUPT);
    create_descriptor( 6, (uint32_t) isr6_entry, 0x08, IDT_TYPE_INTERRUPT);
    create_descriptor( 7, (uint32_t) isr7_entry, 0x08, IDT_TYPE_INTERRUPT);
    create_descriptor( 8, (uint32_t) isr8_entry, 0x08, IDT_TYPE_INTERRUPT);
    create_descriptor(10, (uint32_t)isr10_entry, 0x08, IDT_TYPE_INTERRUPT);
    create_descriptor(11, (uint32_t)isr11_entry, 0x08, IDT_TYPE_INTERRUPT);
    create_descriptor(12, (uint32_t)isr12_entry, 0x08, IDT_TYPE_INTERRUPT);
    create_descriptor(13, (uint32_t)isr13_entry, 0x08, IDT_TYPE_INTERRUPT);
    create_descriptor(14, (uint32_t)isr14_entry, 0x08, IDT_TYPE_INTERRUPT);
    // RSV 15
    create_descriptor(16, (uint32_t)isr16_entry, 0x08, IDT_TYPE_INTERRUPT);
    create_descriptor(17, (uint32_t)isr17_entry, 0x08, IDT_TYPE_INTERRUPT);
    create_descriptor(18, (uint32_t)isr18_entry, 0x08, IDT_TYPE_INTERRUPT);
    create_descriptor(19, (uint32_t)isr19_entry, 0x08, IDT_TYPE_INTERRUPT);
    // RSV 20-28
    create_descriptor(29, (uint32_t)isr29_entry, 0x08, IDT_TYPE_INTERRUPT);
    create_descriptor(30, (uint32_t)isr30_entry, 0x08, IDT_TYPE_INTERRUPT);
    // RSV 31
    // IRQs
    // Master PIC
    create_descriptor(32, (uint32_t) irq0_entry, 0x08, IDT_TYPE_INTERRUPT);
    create_descriptor(33, (uint32_t) irq1_entry, 0x08, IDT_TYPE_TRAP);
    create_descriptor(34, (uint32_t) irq2_entry, 0x08, IDT_TYPE_TRAP);
    create_descriptor(35, (uint32_t) irq3_entry, 0x08, IDT_TYPE_TRAP);
    create_descriptor(36, (uint32_t) irq4_entry, 0x08, IDT_TYPE_TRAP);
    create_descriptor(37, (uint32_t) irq5_entry, 0x08, IDT_TYPE_TRAP);
    create_descriptor(38, (uint32_t) irq6_entry, 0x08, IDT_TYPE_TRAP);
    create_descriptor(39, (uint32_t) irq7_entry, 0x08, IDT_TYPE_TRAP);
    // Slave PIC
    create_descriptor(40, (uint32_t) irq8_entry, 0x08, IDT_TYPE_TRAP);
    create_descriptor(41, (uint32_t) irq9_entry, 0x08, IDT_TYPE_TRAP);
    create_descriptor(42, (uint32_t)irq10_entry, 0x08, IDT_TYPE_TRAP);
    create_descriptor(43, (uint32_t)irq11_entry, 0x08, IDT_TYPE_TRAP);
    create_descriptor(44, (uint32_t)irq12_entry, 0x08, IDT_TYPE_TRAP);
    create_descriptor(45, (uint32_t)irq13_entry, 0x08, IDT_TYPE_TRAP);
    create_descriptor(46, (uint32_t)irq14_entry, 0x08, IDT_TYPE_TRAP);
    create_descriptor(47, (uint32_t)irq15_entry, 0x08, IDT_TYPE_TRAP);
    // Extra IO APIC entries
    create_descriptor(48, (uint32_t)irq16_entry, 0x08, IDT_TYPE_TRAP);
    create_descriptor(49, (uint32_t)irq17_entry, 0x08, IDT_TYPE_TRAP);
    create_descriptor(50, (uint32_t)irq18_entry, 0x08, IDT_TYPE_TRAP);
    create_descriptor(51, (uint32_t)irq19_entry, 0x08, IDT_TYPE_TRAP);
    create_descriptor(52, (uint32_t)irq20_entry, 0x08, IDT_TYPE_TRAP);
    create_descriptor(53, (uint32_t)irq21_entry, 0x08, IDT_TYPE_TRAP);
    create_descriptor(54, (uint32_t)irq22_entry, 0x08, IDT_TYPE_TRAP);
    create_descriptor(55, (uint32_t)irq23_entry, 0x08, IDT_TYPE_TRAP);

    create_descriptor(0x80, (uint32_t)syscall_entry, 0x0B, IDT_TYPE_INTERRUPT);

    // Spurious IRQ entries
    for(size_t i = 0; i < 16; i++)
        create_descriptor(i + 0xF0, (uint32_t)spurious_irq, 0x08, IDT_TYPE_TRAP);
}
