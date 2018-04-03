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
#include <ctype.h>

#include <common/kbd.h>
#include <common/ps2.h>
#include <common/hal.h>
#include <common/sched.h>
#include <common/keycodes.h>

#define MOD_SCROLL_LOCK 0x01
#define MOD_NUM_LOCK 0x02
#define MOD_CAPS_LOCK 0x04
#define MOD_SHIFT 0x80

static uint8_t input_buffer[4096];
static uint16_t read_head = 0;
static uint16_t write_head = 0;
uint8_t key_mods = 0;
uint8_t key_states[0xFF];
key_mapping_t default_charmap[] = {KEYCHAR_MAP_DEFAULT};
key_mapping_t *charmap;
static bool caps_pressed = false;
static bool is_inited = false;
static bool has_data = false;

static void input_push(uint8_t keycode)
{
	if(write_head >= 4096) write_head = 0;
	input_buffer[write_head++] = keycode;
}

static uint8_t input_pop()
{
	if(read_head == write_head) return 0;
	else if(read_head >= 4096) read_head = 0;
	return input_buffer[read_head++];
}

void kbd_init()
{
	charmap = default_charmap;
	is_inited = true;
}

void kbd_write(uint8_t keycode)
{
	has_data = true;
	input_push(keycode);
}

uint8_t kbd_read()
{
	uint8_t keycode = 0;

	recheck:
	keycode = input_pop();
	if(keycode == 0)
	{
		// TODO: Either add "interruptable" sleep, or add thread to wakeup queue
		has_data = false;
		//while(!has_data) sched_sleep_millis(0);

		//goto recheck;
	}
	return keycode;
}

void kbd_setstate(uint8_t keycode, uint8_t state)
{
	key_states[keycode] = state;
}

uint8_t kbd_getstate(uint8_t keycode)
{
	return key_states[keycode];
}

void kbd_setmods(uint8_t modifiers)
{
	key_mods = modifiers;
}

uint8_t kbd_getmods()
{
	return key_mods;
}

void kbd_loadmap(key_mapping_t *mapping)
{
	charmap = mapping;
}

bool kbd_handle_key(uint8_t keycode, bool released)
{
	if(!is_inited) return false;

	uint8_t new_kmods = key_mods;
	if(!released || keycode == KEY_PAUSE)
	{
		kbd_write(keycode);
		if(kbd_getstate(keycode) == KEY_STATE_PRESSED)
			kbd_setstate(keycode, KEY_STATE_REPEAT);
		else
			kbd_setstate(keycode, KEY_STATE_PRESSED);

		if(keycode == KEY_L_SHIFT || keycode == KEY_R_SHIFT)
			new_kmods |= MOD_SHIFT;
		else if(keycode == KEY_CAPSLOCK && ((new_kmods & MOD_CAPS_LOCK) == 0))
		{
			new_kmods |= MOD_CAPS_LOCK;
			caps_pressed = true;
		}
	} else if(released)
	{
		kbd_setstate(keycode, KEY_STATE_RELEASED);

		if(keycode == KEY_L_SHIFT || keycode == KEY_R_SHIFT)
			new_kmods &= ~MOD_SHIFT;
		else if(keycode == KEY_CAPSLOCK && (new_kmods & MOD_CAPS_LOCK) && !caps_pressed)
		{
			new_kmods &= ~MOD_CAPS_LOCK;
		}
		else if(keycode == KEY_CAPSLOCK)
			caps_pressed = false;
	}
	if(new_kmods != key_mods)
	{
		key_mods = new_kmods;
		kbd_setmods(key_mods);
		return true;
	}

	return false;
}

char kbd_tochar(uint8_t keycode)
{
	if(keycode > KEY_KPDOT) return 0;

	if(keycode >= KEY_A && keycode <= KEY_Z)
	{
		if(key_mods & MOD_SHIFT && key_mods & MOD_CAPS_LOCK)
			return charmap[keycode].normal_char;
		else if(key_mods & MOD_SHIFT || key_mods & MOD_CAPS_LOCK)
			return charmap[keycode].shift_char;
		else
			return charmap[keycode].normal_char;
	} else
	{
		if(key_mods & MOD_SHIFT)
			return charmap[keycode].shift_char;
		else
			return charmap[keycode].normal_char;
	}
}

uint8_t key_get_state(uint8_t keycode)
{
	if(keycode == KEY_PAUSE && key_states[keycode])
	{
		key_states[keycode] = KEY_STATE_RELEASED;
		return KEY_STATE_PRESSED;
	}
	return key_states[keycode];
}
