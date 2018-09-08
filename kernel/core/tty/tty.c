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

#include <common/hal.h>
#include <common/tty/tty.h>
#include <common/tty/fb.h>
#include <common/mm/mm.h>
#include <common/io/uart.h>

#define TTY_SIZE 80*25*1
#define EMPTY_CHAR (' ')

int16_t width = 0;
int16_t height = 0;
int16_t column = 0;
int16_t row = 0;
// Used for offset
uint16_t screen_row = 0;
uint16_t last_draw = 0;
tty_colour_t colour = {.fg_colour=0x7,.bg_colour=0x0};
tty_device_t extra_devices[2];
tty_char_t window[TTY_SIZE];
bool background_reshow = false;

static uint32_t ega2clr[] = {
    0x000000, 0x0000AA, 0x00AA00, 0x00AAAA, 0xAA0000, 0xAA00AA, 0xAA5500, 0xAAAAAA,
    0x555555, 0x5555FF, 0x55FF55, 0x55FFFF, 0xFF5555, 0xFF55FF, 0xFFFF55, 0xFFFFFF,
};

static size_t strlen(const char* str)
{
    size_t len = 0;
    while(str[len]) len++;
    return len;
}

static void tty_refresh_cursor()
{
    size_t index = column + (row + screen_row) * width;
    window[index].actual_char = '_';
    window[index].colour = colour;
}

/*
 * Initializes TTY Constants
 */
void tty_init()
{
    // TODO: Make these function parameters
    width = 80;
    height = 25;
    extra_devices[0].base = (size_t)KNULL;
    extra_devices[1].base = (size_t)KNULL;
    // TODO: memset tty_window
    tty_clear();
}

void tty_prints(const char* str)
{
    // Print string to tty window
    for(int i = 0; i < strlen(str); i++)
        tty_printchar(str[i]);
    tty_reshow();
}

static void tty_tabput()
{
    for(int i = 0; i < 4; i++)
    {
        size_t index = column + (row + screen_row) * width;
        window[index].actual_char = '\t';
        window[index].colour = colour;

        if(++column >= width)
        {
            column = 0;
            if(++row >= height)
            {
                row = height - 1;
                tty_scroll();
            }
        }
    }

    tty_refresh_cursor();
}

static void tty_tabunput()
{
    for(int i = 0; i < 4; i++)
    {
        if(--column < 0)
        {
            if(row > 0) column = width-1;
            else column = 0;
            if(--row < 0) row = 0;
        }

        size_t index = column + (row + screen_row) * width;
        window[index].actual_char = EMPTY_CHAR;
        window[index].colour = colour;
    }

    tty_refresh_cursor();
}

void tty_printchar(const char c)
{
    char pchar = c;
    size_t index = column + (row + screen_row) * width;

    if(c == '\t')
    {
        tty_tabput();
        return;
    }

    if(c == '\b')
    {
        pchar = EMPTY_CHAR;

        window[index].actual_char = EMPTY_CHAR;
        if(window[index-1].actual_char == '\t')
        {
            tty_tabunput();
            return;
        }

        if(--column < 0)
        {
            if(row > 0) column = width-1;
            else column = 0;
            if(--row < 0) row = 0;
        }
        background_reshow = true;
    }
    if(c == '\n') pchar = '\n';

    // Print char to tty window
    index = column + (row + screen_row) * width;
    window[index].actual_char = pchar;
    window[index].colour = colour;

    if(c == '\b')
    {
        tty_refresh_cursor();
        return;
    } else if(c == '\n' || ++column >= width)
    {
        column = 0;
        if(++row >= height)
        {
            row = height - 1;
            tty_scroll();
        }
    }

    tty_refresh_cursor();
}

void tty_scroll()
{
    size_t line = (screen_row) * width;
    uint16_t* raw_window = (uint16_t*)window;

    for(int y = 1; y < height; y++)
    {
        for(int x = 0; x < width; x++)
        {
            raw_window[line+x] = raw_window[line+width+x];
        }
        line += width;
    }

    for(int x = 0; x < width; x++)
    {
        ((uint16_t*)window)[x+(height-1)*width] = 0x0000;
    }
    last_draw -= width;

    background_reshow = true;
}

void tty_set_colour(uint8_t fg, uint8_t bg)
{
    colour.fg_colour = fg;
    colour.bg_colour = bg;
}

/*
 * Repaints screen to display devicees
 */
void tty_reshow()
{
    hal_save_interrupts();
    uint16_t current_draw = (column + (row + screen_row) * width);

    // Write to UART
    for(uint16_t uart_base = last_draw; uart_base < current_draw; uart_base++)
    {
        if(    window[uart_base].actual_char < ' ' &&
            window[uart_base].actual_char != '\n' &&
            window[uart_base].actual_char != '\r') continue;
        if(uart_base >= TTY_SIZE)
        {
            uart_base = TTY_SIZE - width;
        }
        else if(window[uart_base].actual_char == '\n')
        {
            uart_writec('\n');
            uart_writec('\r');
        }
        else {
            /*if((uart_base % width) == width - 1)
            {
                uart_writec('\n');
                uart_writec('\r');
            }*/
            uart_writec(window[uart_base].actual_char);
        }
    }

    if(extra_devices[VGA_CONSOLE].base != (size_t)KNULL &&
        mmu_is_usable(extra_devices[VGA_CONSOLE].base))
    {
        // EGA Textmode console
        uint16_t* console_base = (uint16_t*)extra_devices[VGA_CONSOLE].base;
        int i = 0;

        if(background_reshow) i = 0;
        else i = last_draw;

        for(; i < current_draw; i++)
        {
            unsigned char pchar = window[i].actual_char;
            if( window[i].actual_char == '\t' ||
                window[i].actual_char == 0)
            {
                pchar = EMPTY_CHAR;
            }
            else if(window[i].actual_char == '\n')
            {
                // Skip 0 bytes
                i -= i % width - (width - 1);
                continue;
            }
            console_base[i] = (window[i].colour.bg_colour << 12) | (window[i].colour.fg_colour << 8) | pchar;
        }

        // Erase extra charachters
        for(int i = current_draw; i < last_draw && last_draw > current_draw; i++)
        {
            console_base[i] = (window[i].colour.bg_colour << 12) | (window[i].colour.fg_colour << 8) | EMPTY_CHAR;
        }
    }

    if(extra_devices[FB_CONSOLE].base != (size_t)KNULL &&
        mmu_is_usable(extra_devices[FB_CONSOLE].base))
    {
        int i = 0;
        if(background_reshow) i = 0;
        else i = last_draw;

        for(; i < current_draw; i++)
        {
            unsigned char pchar = window[i].actual_char;
            if(    pchar == '\t')
            {
                pchar = EMPTY_CHAR;
            }
            else if(window[i].actual_char == '\n')
            {
                i -= i % width - (width - 1);
                continue;
            }
            else if(pchar == 0)
            {
                continue;
            }

            fb_fill_putchar((void*)extra_devices[FB_CONSOLE].base,
                        (i % width) << 3,
                        (i / width) << 4,
                        pchar,
                        ega2clr[window[i].colour.fg_colour],
                        ega2clr[window[i].colour.bg_colour]);
        }

        // Erase extra charachters
        for(int i = current_draw; i < last_draw && last_draw > current_draw; i++)
        {
            fb_fill_putchar((void*)extra_devices[FB_CONSOLE].base,
                        (i % width) << 3,
                        (i / width) << 4,
                        EMPTY_CHAR,
                        ega2clr[window[i].colour.fg_colour],
                        ega2clr[window[i].colour.bg_colour]);
        }
    }

    last_draw = current_draw;
    hal_restore_interrupts();
}

/*
 * Adds an output device for the tty to output to
 */
void tty_add_output(enum OutputType type, size_t base)
{
    switch (type) {
#ifndef __K4_VISUAL_STACK__
        case VGA_CONSOLE:
            extra_devices[VGA_CONSOLE].base = base;
            break;
        case FB_CONSOLE:
            extra_devices[FB_CONSOLE].base = base;
            break;
#endif
        case SERIAL: // Serial is always enabled
        default:
            break;
    }
}

bool tty_background_dirty()
{
    return background_reshow;
}

void tty_make_clean()
{
    background_reshow = false;
}

void tty_clear()
{
    for(int i = 0; i < TTY_SIZE; i++)
    {
        window[i].colour = colour;
        window[i].actual_char = 0x00;
    }

    column = 0;
    row = 0;
    last_draw = 0;
    screen_row = 0;
    background_reshow = true;
}

/**
 * Redraws the entire display to output devices
 */
void tty_reshow_full()
{
    last_draw = 0;
    tty_reshow();
}
