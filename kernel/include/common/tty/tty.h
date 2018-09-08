#include <common/types.h>

#ifndef __TTY_H__
#define __TTY_H__

enum OutputType {
    VGA_CONSOLE = 0,
    FB_CONSOLE = 1,
    SERIAL = 2,
};

typedef struct {
    uint8_t fg_colour : 4;
    uint8_t bg_colour : 4;
} tty_colour_t;

typedef struct {
    tty_colour_t colour;
    char actual_char;
} tty_char_t;

typedef struct {
    size_t base;
} tty_device_t;

void tty_init();
void tty_scroll();
void tty_prints(const char* str);
void tty_printchar(const char c);
void tty_set_colour(uint8_t fg, uint8_t bg);
void tty_reshow();
void tty_reshow_full();
void tty_add_output(enum OutputType type, size_t base);
bool tty_background_dirty();
void tty_make_clean();
void tty_clear();

#endif /* __TTY_H__ */
