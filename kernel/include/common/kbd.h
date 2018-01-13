#include <types.h>

#ifndef __KEYBOARD_H__
#define __KEYBOARD_H__

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
void kbd_loadmap(key_mapping_t *mapping);
char kbd_tochar(uint8_t keycode);


#endif /* __KEYBOARD_H__ */
