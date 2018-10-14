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
#include <arch/pic.h>
#include <arch/idt.h>
#include <arch/io.h>
#include <stack_state.h>

#define PIC1_CMD   0x20
#define PIC1_DATA  0x21
#define PIC2_CMD   0xA0
#define PIC2_DATA  0xA1

// ICWs
#define ICW1_INIT 0x10 // Init start
#define ICW1_IC4  0x01 // (Don't) Read ICW4
#define ICW1_SNGL 0x02 // Single (Cascade) Mode
#define ICW1_ADI  0x04 // 8bit (4bit) CALL Interval
#define ICW1_LTIM 0x08 // (Not) Level interrupt mode
// ICW2 is on DATA port (Vector base)
// ICW3 is on DATA port (Cascade configure)
// ICW4 is on DATA port (Vector base)
#define ICW4_MPM  0x01 // MCS (8086) Mode
#define ICW4_AEOI 0x02 // Normal (Auto) EOI
#define ICW4_BUF  0x04 // (Buffered)/Unbuffered Mode
#define ICW4_MSS  0x08 // (Master)/Slave Type
#define ICW4_SFNM 0x01 // Specially fully nested mode

// OCWs
#define OCW2_EOI  0x20 // EOI current IRQ
#define OCW2_SEL  0x40 // Select specific IRQ
#define OCW2_ROT  0x80 // Rotate priorities

#define OCW3_RREG 0x01 // Read Register
#define OCW3_R_IS 0x02 // Read Interrupt Status Register
#define OCW3_POLL 0x04 // Poll command
#define OCW3_WREN 0x08 // Write to OCW3
#define OCW3_SSMM 0x20 // Set Special Mask
#define OCW3_ESMM 0x40 // Enable writes to Special Mask

static void irq_wrapper(struct stack_state* state, void* params)
{
    ((irq_function_t)params)(pic_get_dev());
}

uint32_t pic_read_irr()
{
    uint16_t irr = 0;
    outb(PIC1_CMD, OCW3_WREN | OCW3_RREG);
    irr |= inb(PIC1_CMD);
    outb(PIC2_CMD, OCW3_WREN | OCW3_RREG);
    irr |= inb(PIC2_CMD) << 8;
    return (uint32_t)irr;
}

uint32_t pic_read_imr()
{
    return (uint32_t)(inb(PIC1_DATA) | (inb(PIC2_DATA) << 8));
}

void pic_setup(uint8_t irq_base)
{
    // Save Masks
    uint8_t mask_a = inb(PIC1_DATA);
    uint8_t mask_b = inb(PIC2_DATA);

    // (ICW1) Init PICS
    outb(PIC1_CMD, ICW1_INIT | ICW1_IC4);
    io_wait();
    outb(PIC2_CMD, ICW1_INIT | ICW1_IC4);
    io_wait();

    // (ICW2) Set vector bases
    outb(PIC1_DATA, irq_base);
    io_wait();
    outb(PIC2_DATA, irq_base + 8);
    io_wait();

    // (ICW3) Cascade Setup (On IRQ 2)
    outb(PIC1_DATA, 0b00000100);
    io_wait();
    outb(PIC2_DATA, 0x02);
    io_wait();

    // (ICW4) 8086 Mode
    // Do we need AEOI?
    outb(PIC1_DATA, ICW4_MPM);
    io_wait();
    outb(PIC2_DATA, ICW4_MPM);
    io_wait();

    // Restore Masks
    outb(PIC1_DATA, mask_a);
    outb(PIC2_DATA, mask_b);
}

void pic_disable()
{
}

void pic_mask(uint8_t irq)
{
    uint8_t port;
    if(irq >= 8)
    {
        port = PIC2_DATA;
        irq -= 8;
    } else
    {
        port = PIC1_DATA;
    }

    uint8_t mask = inb(port) | (1 << irq);
    outb(port, mask);
}

void pic_unmask(uint8_t irq)
{
    uint8_t port;
    if(irq >= 8)
    {
        port = PIC2_DATA;
        irq -= 8;
    } else
    {
        port = PIC1_DATA;
    }

    uint8_t mask = inb(port) & ~(1 << irq);
    outb(port, mask);
}

bool pic_check_spurious(uint8_t irq)
{
    if(irq != 7 && irq != 15) return false;
    uint32_t irr = pic_read_irr();

    if(irq == 7  && ~(irr & 0x08))
    {
        return true;
    } else if(irq == 15 && ~(irr & 0x80))
    {
        outb(PIC1_CMD, OCW2_EOI);
        return true;
    }
    return false;
}

void pic_eoi(uint8_t irq)
{
    if(irq >= 8) outb(PIC2_CMD, OCW2_EOI);
    outb(PIC1_CMD, OCW2_EOI);
}

int pic_alloc_irq(uint8_t irq)
{
    return 0;
}

int pic_free_irq(uint8_t irq)
{
    return 0;
}

int pic_handle_irq(uint8_t irq, irq_function_t handler)
{
    isr_add_handler(irq + IRQ_BASE, irq_wrapper, handler);

    pic_unmask(irq);
    if(irq >= 8)
        pic_unmask(2);
    return 0;
}

struct ic_dev pic_dev = {
    .enable = pic_setup,
    .disable = pic_disable,
    .mask = pic_mask,
    .unmask = pic_unmask,
    .is_spurious = pic_check_spurious,
    .eoi = pic_eoi,
    .alloc_irq = pic_alloc_irq,
    .free_irq = pic_free_irq,
    .handle_irq = pic_handle_irq,
    .get_mask = pic_read_imr,
    .get_serviced = pic_read_irr,
};

struct ic_dev* pic_get_dev()
{
    return &pic_dev;
}
