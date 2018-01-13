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

static uint8_t keycode_buffer[4096];
static int16_t buffer_off = -1;
/*
 * 0: xF0 Flag
 * 1: xE0 Flag
 * 2: xE1 Flag
 * 3: Finish Flag
 */
static uint8_t key_state_machine = 0;
// 7 = shift
static uint8_t key_mods = 0;
static uint8_t ps2set2_translation[] = {PS2_SET2_MAP};
bool caps_pressed = false;

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
	ps2_device_write(0, true, command);
	if(ps2_device_read(0, true) != 0xFA) return;
	ps2_device_write(0, true, subcommand);
	if(ps2_device_read(0, true) != 0xFA) return;
}

isr_retval_t ps2_keyboard_isr()
{
	uint8_t data = ps2_device_read(0, false);
	if(data == 0xFA) goto keep_consume;

	switch(data)
	{
		case 0xF0: key_state_machine |= 0b0001; break;
		case 0xE0: key_state_machine |= 0b0010; break;
		case 0xE1: key_state_machine |= 0b0100; break;
		case 0x77:
			if(key_state_machine == 0b0101)
			{
				key_state_machine |= 0b1000;
				break;
			}
		case 0x7C:
			if((key_state_machine & 0b0010) && (key_state_machine & 0b0001) == 0)
			{
				key_state_machine |= 0b1000;
				break;
			} else if((key_state_machine & 0b0010) && (key_state_machine & 0b0001) == 1)
			{
				key_state_machine &= ~0b1011;
				break;
			}
		case 0x12:
			if((key_state_machine & 0b0010) && (key_state_machine & 0b0001) == 1)
			{
				key_state_machine |= 0b1000;
				data = 0x7C;
				break;
			}
			else if((key_state_machine & 0b0010) && (key_state_machine & 0b0001) == 0)
			{
				key_state_machine &= ~0b1011;
				break;
			}
		default:
			if((key_state_machine & 0b0100) == 0)
				key_state_machine |= 0b1000;
			break;
	}
	if((key_state_machine & 0b1000) == 0) goto keep_consume;
	uint8_t keycode = KEY_RESERVED;
	if(key_state_machine & 0b0110)
		keycode = ps2set2_translation[data + 0x80];
	else
		keycode = ps2set2_translation[data + 0x00];

	uint8_t new_kmods = key_mods;
	if((key_state_machine & 0b0001) == 0 || keycode == KEY_PAUSE)
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
	} else if(key_state_machine & 1)
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
		send_command(0xED, key_mods & 0xf);
	}
	key_state_machine = 0;

	keep_consume:
	ic_eoi(ps2_device_irqs()[0]);
	return ISR_HANDLED;
}

void ps2kbd_init()
{
	// PS2 Side
	ps2_handle_device(0, ps2_keyboard_isr);
	send_command(0xF0, 0x02);
	send_command(0xED, 0x0f);
}
