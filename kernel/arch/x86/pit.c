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

#include <arch/pit.h>
#include <arch/idt.h>
#include <arch/io.h>

#define PIT_MODE_ONESHOT          (0 << 1) // Interrupts when the counter reaches zero
#define PIT_MODE_HARDWARE_ONESHOT (1 << 1) // Start upon gate input (Speaker port only)
#define PIT_MODE_PERIODIC         (2 << 1) // Counter acts as a frequency divider
#define PIT_MODE_PERIODIC_SQWAVE  (3 << 1) // Counter acts as a frequency divider, only half the time
#define PIT_MODE_SOFTWARE_STROBE  (4 << 1) // Retriggerable delay (+55ms range)
#define PIT_MODE_HARDWARE_STROBE  (5 << 1) // Retriggerable delay activated by gate input (Speaker port only)

#define PIT0_DATA 0x40
#define PIT1_DATA 0x41
#define PIT2_DATA 0x42
#define PIT_MCR   0x43

#define PIT_FREQ (1193181)

struct pit_timer_dev
{
    struct timer_dev raw_dev;
    uint8_t timer_mode;
};

struct pit_timer_dev timer_devs[2];

static void pit_handler()
{
    // We EOI here as sched_timer may switch to another task that is the only one and never returns.
    ic_eoi(0);
    struct pit_timer_dev* timer = &(timer_devs[0]);
    timer->raw_dev.counter += timer->raw_dev.resolution;
    timer_broadcast_update(timer->raw_dev.id);
}

void pit_init_counter(uint16_t id, uint16_t frequency, uint8_t mode)
{
    uint16_t reload = (uint16_t)(PIT_FREQ/frequency);
    uint16_t data_port;
    uint8_t config_byte = 0;

    // Timer ID's zero to one are the only accessable one, any others map to the main one
    if(id > 1)
        id = 0;

    data_port = PIT0_DATA | id << 1;

    // Set frequency range
    if(frequency <= 18)
    {
        reload = 0;     // Same as 65536
        frequency = 18;
    }
    if(frequency >= PIT_FREQ)
    {
        reload = 1;
        frequency = PIT_FREQ;
    }

    config_byte |= mode;                // Timer Mode
    config_byte |= id << 7;             // This maps ids 0, 1 to 0, 2, whilst also setting the select
    config_byte |= (0b11 << 4);         // Lo-hibyte access mode
    timer_devs[id].timer_mode = config_byte;

    // Send config byte
    outb(PIT_MCR, config_byte);

    // Send the counter reload value
    outb(data_port, (uint8_t)(reload & 0xFF));
    outb(data_port, (uint8_t)(reload >> 8));

    timer_devs[id].raw_dev.resolution = 1000000000 / (PIT_FREQ / frequency);
}

void pit_reset_counter(uint16_t id)
{
    if(id > 1) id = 0;
    outb(PIT_MCR, timer_devs[id].timer_mode);
}

void pit_set_counter(uint16_t id, uint16_t frequency)
{
    if(id > 1) id = 0;
    uint16_t data_port = PIT0_DATA | (id << 1);

    uint16_t reload = (uint16_t)(PIT_FREQ/frequency);
    outb(data_port, (uint8_t)(reload & 0xFF));
    outb(data_port, (uint8_t)(reload >> 8));
}

void pit_init()
{
    pit_init_counter(0, 1000, PIT_MODE_PERIODIC);
    timer_add(&(timer_devs[0]), PERIODIC);
    timer_set_default(timer_devs[0].raw_dev.id);
    isr_add_handler(IRQ_BASE, pit_handler);
}
