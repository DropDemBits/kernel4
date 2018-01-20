#include <kbd.h>
#include <ps2.h>
#include <hal.h>
#include <ctype.h>
#include <keycodes.h>
#include <io.h>

#define MOD_SCROLL_LOCK 0x01
#define MOD_NUM_LOCK 0x02
#define MOD_CAPS_LOCK 0x04
#define MOD_SHIFT 0x80

uint8_t input_buffer[4096];
int16_t input_off = -1;
uint8_t key_mods = 0;
uint8_t key_states[0xFF];
key_mapping_t default_charmap[] = {KEYCHAR_MAP_DEFAULT};
key_mapping_t *charmap;
static bool caps_pressed = false;

static void input_push(uint8_t keycode)
{
	if(input_off >= 4095)
		input_off = 4095;
	else
		input_buffer[++input_off] = keycode;
}

static uint8_t input_pop()
{
	if(input_off < 0) return 0;
	return input_buffer[input_off--];
}

void kbd_init()
{
	charmap = default_charmap;
}

void kbd_write(uint8_t keycode)
{
	input_push(keycode);
}

uint8_t kbd_read()
{
	return input_pop();
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
