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

#ifndef __KEYBOARD_H__
#define __KEYBOARD_H__ 1

#include <common/types.h>

#define KEY_STATE_RELEASED 0
#define KEY_STATE_PRESSED 1
#define KEY_STATE_REPEAT 2

typedef struct
{
    char normal_char;
    char shift_char;
    char altgr_char;
} key_mapping_t;

void kbd_init();
void kbd_write(uint8_t keycode);
uint8_t kbd_read();
void kbd_setstate(uint8_t keycode, uint8_t state);
uint8_t kbd_getstate(uint8_t keycode);
uint8_t kbd_getmods();
bool kbd_handle_key(uint8_t keycode, bool released);
void kbd_loadmap(key_mapping_t *mapping);
char kbd_tochar(uint8_t keycode);


#endif /* __KEYBOARD_H__ */
