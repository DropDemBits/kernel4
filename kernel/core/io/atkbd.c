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

#include <ctype.h>
#include <common/kbd.h>
#include <common/ps2.h>
#include <common/hal.h>
#include <common/keycodes.h>

#define MOD_SCROLL_LOCK 0x01
#define MOD_NUM_LOCK 0x02
#define MOD_CAPS_LOCK 0x04
#define MOD_SHIFT 0x80

static uint8_t keycode_buffer[4096];
static int16_t buffer_off = -1;
static int kbd_device = 0;
/*
 * 0: Special Flag
 * 1: xE0 Flag
 * 2: xE1 Flag
 * 3: Finish Flag
 */
static uint8_t key_state_machine = 0;
static uint8_t ps2set1_translation[] = {PS2_SET1_MAP};

static void keycode_push(uint8_t keycode)
{
	if(buffer_off >= 4095)
		buffer_off = 4095;
	else
		keycode_buffer[++buffer_off] = keycode;
}

static uint8_t keycode_pop()
{
	if(buffer_off < 0) return 0;
	return keycode_buffer[buffer_off--];
}

static void send_command(uint8_t command, uint8_t subcommand)
{
	ps2_device_write(kbd_device, true, command);
	if(ps2_device_read(kbd_device, true) != 0xFA) return;
	ps2_device_write(kbd_device, true, subcommand);
	if(ps2_device_read(kbd_device, true) != 0xFA) return;
}

static isr_retval_t at_keyboard_isr()
{
	uint8_t data = ps2_device_read(kbd_device, false);
	if(data == 0xFA) goto keep_consume;

	switch(data)
	{
		case 0xE0: key_state_machine |= 0b0010; break;
		case 0xE1: key_state_machine |= 0b0100; break;
		// Print screen
		case 0x2A:
			if(key_state_machine & 0b0010)
			{
				key_state_machine |= 0b0001;
				break;
			}
		case 0xB7:
			if(key_state_machine & 0b0010)
			{
				key_state_machine |= 0b0001;
				break;
			}
		// Pause
		case 0x1D:
			if(key_state_machine & 0b0100)
			{
				key_state_machine = 0b0111;
				break;
			}
		case 0xC5:
			if(key_state_machine == 0b0111)
			{
				key_state_machine |= 0b1000;
				break;
			}
		default:
			if((key_state_machine & 0b0110) < 0b0100)
				key_state_machine |= 0b1000;
			break;
	}

	if((key_state_machine & 0b1000) == 0) goto keep_consume;

	uint8_t keycode = KEY_RESERVED;
	if(key_state_machine == 0xa)
	{
		keycode = ps2set1_translation[(data & 0x7F) + 0x50];
	}
	else if(key_state_machine == 0xf)
	{
		keycode = ps2set1_translation[data];
	}
	else if(key_state_machine == 0xb)
	{
		if(data == 0xAA) data = 0xB7;
		keycode = ps2set1_translation[(data & 0x7F) + 0x90];
	}
	else
	{
		keycode = ps2set1_translation[(data & 0x7F) + 0x00];
	}
	key_state_machine = 0;

	if(kbd_handle_key(keycode, data & 0x80))
		send_command(0xED, kbd_getmods() & 0xf);

	keep_consume:
	ic_eoi(ps2_device_irqs()[kbd_device]);
	return ISR_HANDLED;
}

void atkbd_init(int device)
{
	// PS2 Side
	kbd_device = device;
	ps2_device_write(kbd_device, true, 0xF4);
	if(ps2_device_read(kbd_device, true) != 0xFA)
		puts("[KBD] Scanning enable failed");
	ps2_handle_device(kbd_device, at_keyboard_isr);
	send_command(0xF0, 0x01);
	send_command(0xF3, 0x20);
	send_command(0xED, 0x00);
}
