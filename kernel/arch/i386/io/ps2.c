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

#include <common/ps2.h>
#include <arch/io.h>

uint8_t irqs[] = {1, 12};

void ps2_controller_writeb(uint8_t data)
{
    outb(0x64, data);
}

void ps2_device_writeb(uint8_t data)
{
    outb(0x60, data);
}

uint8_t ps2_read_status()
{
    return inb(0x64);
}

uint8_t ps2_read_data()
{
    return inb(0x60);
}

uint8_t* ps2_device_irqs()
{
    return irqs;
}

void ps2_wait()
{
    io_wait();
    asm("hlt");
}
