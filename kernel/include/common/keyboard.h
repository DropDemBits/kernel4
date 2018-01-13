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

void keyboard_init();
uint8_t keyboard_read_key();
void keyboard_load_map(key_mapping_t *mapping);
char keyboard_tochar(uint8_t keycode);


#endif /* __KEYBOARD_H__ */
