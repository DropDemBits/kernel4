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

#include <i386/pit.h>
#include <i386/io.h>

#define PIT0_DATA 0x40
#define PIT1_DATA 0x41
#define PIT2_DATA 0x42
#define PIT_MCR   0x43

uint8_t mode_bytes[2];

void pit_init_counter(uint16_t id, uint16_t frequency, uint8_t mode)
{
	uint16_t reload = (uint16_t)(TM_FREQ/frequency);
	if(frequency <= 18)
		reload = 0;
	uint16_t data_port = PIT0_DATA;
	uint8_t config_byte = 0;

	if(id > 1) id = 0;

	if(mode == TM_MODE_INTERVAL) config_byte |= (2 << 1);
	else if(mode == TM_MODE_ONESHOT) config_byte |= (0 << 1);

	if(id == 1)
	{
		config_byte |= (0b10) << 6;
		data_port = PIT2_DATA;
		id--;
	} else
	{
		config_byte |= (0b00) << 6;
	}
	config_byte |= (0b11 << 4);
	mode_bytes[id] = config_byte;

	outb(PIT_MCR, config_byte);
	outb(data_port, (uint8_t)(reload & 0xFF));
	outb(data_port, (uint8_t)(reload >> 8));
}

void pit_reset_counter(uint16_t id)
{
	if(id > 1) id = 0;
	outb(PIT_MCR, mode_bytes[id]);
}

void pit_set_counter(uint16_t id, uint16_t frequency)
{
	uint16_t data_port = PIT0_DATA;
	if(id == 1) data_port = PIT2_DATA;

	uint16_t reload = (uint16_t)(TM_FREQ/(float)frequency);
	outb(data_port, (uint8_t)(reload & 0xFF));
	outb(data_port, (uint8_t)(reload >> 8));
}
