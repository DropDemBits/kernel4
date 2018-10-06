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
#include <string.h>

#include <common/hal.h>
#include <common/tty/tty.h>
#include <common/tty/fb.h>
#include <common/mm/mm.h>
#include <common/io/uart.h>

#define TTY_SIZE 80*25*1
#define EMPTY_CHAR (' ')

tty_dev_t* active_tty = NULL;

static uint32_t ega_palette[] = {
    0x000000, 0x0000AA, 0x00AA00, 0x00AAAA, 0xAA0000, 0xAA00AA, 0xAA5500, 0xAAAAAA,
    0x555555, 0x5555FF, 0x55FF55, 0x55FFFF, 0xFF5555, 0xFF55FF, 0xFFFF55, 0xFFFFFF,
};

/*
 * Initializes TTY Constants
 */
void tty_init(tty_dev_t* tty, uint16_t width, uint16_t height, void* buffer, size_t buffer_len, tty_colour_t* palette)
{
    tty->width = width;
    tty->height = height;
    tty->buffer_base = buffer;
    tty->buffer_size = buffer_len >> 1;
    tty->current_idx = 0;
    tty->last_idx = 0;
    tty->is_dirty = false;
    tty->refresh_back = false;
    tty->scrollback_limit = 0;
    tty->mutex = mutex_create();

    if(palette == NULL)
    {
        tty->default_palette = ega_palette;
        tty->current_palette = ega_palette;
    }
    else
    {
        tty->default_palette = (uint32_t*)palette;
        tty->current_palette = (uint32_t*)palette;
    }

    tty_clear(tty, true);
    tty_set_colours(tty, 0x07, 0x00);
}

void tty_puts(tty_dev_t* tty, const char* str)
{
    // Print string to tty
    for(int i = 0; i < strlen(str); i++)
        tty_putchar(tty, str[i]);
}

static void putchar_raw(tty_dev_t* tty, const char c, int column_delta)
{
    // Print char to tty
    uint32_t index = (tty->column + (tty->row * tty->width) + tty->draw_base) % tty->buffer_size;
    tty->buffer_base[index].actual_char = c;
    tty->buffer_base[index].colour = tty->default_colour;

    if(c == '\n' || (tty->column += column_delta) >= tty->width)
    {
        tty->column = 0;
        if(++(tty->row) >= tty->height)
        {
            tty->row = tty->height - 1;
            tty_scroll(tty, 1);
        }
    }

    // Current index is the next char index to be printed
    tty->current_idx = tty->column + (tty->row * tty->width);
}

void tty_putchar(tty_dev_t* tty, const char c)
{
    mutex_acquire(tty->mutex);

    char printed_char = c;
    int column_delta = 1;
    uint32_t index = (tty->column + (tty->row * tty->width) + tty->draw_base) % tty->buffer_size;

    if(c == '\t')
    {
        // Put tab there (leave last one to be updated by the actual methods below)
        for(int i = 0; i < 3; i++)
            putchar_raw(tty, printed_char, column_delta);
    }

    if(c == '\b')
    {
        // Don't erase if it is the last charachter
        if(tty->column == 0 && tty->row == 0)
            return;

        // Erase last charachter
        printed_char = 0;
        column_delta = -1;

        if(tty->buffer_base[index-1].actual_char == '\t')
        {
            // Erase tab
            for(int i = 0; i < 3; i++)
                putchar_raw(tty, printed_char, column_delta);
        }
    }

    // Print the actual charachter
    putchar_raw(tty, printed_char, column_delta);

    mutex_release(tty->mutex);
}

// Because display_base is not bounded by buffer size, it will overflow.
// However, this should not be worrisome if the tty is used for short-term use (on the order of days to years)
bool tty_scroll(tty_dev_t* tty, int direction)
{
    bool has_changed = false;

    // Don't scroll if we hit the scrollback limit
    if (tty->scrollback_limit == (tty->display_base % tty->buffer_size) &&
        direction < 0)
        return has_changed;

    if(tty->draw_base == tty->display_base && direction > 0)
    {
        // Move draw base down
        tty->draw_base += (tty->width * direction);
    }

    if(direction == 0)
    {
        // Reshow back if we have just moved
        has_changed = tty->display_base != tty->draw_base;
        
        // Move display base to draw base
        tty->display_base = tty->draw_base;
    }
    else
    {
        // Apply normal scroll
        tty->display_base += (tty->width * direction);
        has_changed = true;
    }

    if((tty->draw_base + tty->width * tty->height) % tty->buffer_size == tty->scrollback_limit)
    {
        // If we hit the scrollback limit going down, clear last row and adjust scrollback limit
        int row_base = tty->scrollback_limit;
        memset(tty->buffer_base + row_base, 0, tty->width << 1);

        tty->scrollback_limit += tty->width;
        tty->scrollback_limit %= tty->buffer_size;
    }

    if(has_changed || direction != 0)
    {
        tty->refresh_back = true;
        tty->just_scrolled = true;
    }

    return has_changed;
}

void tty_set_cursor(tty_dev_t* tty, int x, int y, bool relative)
{
    if(relative)
    {
        tty->column += (int32_t)x;
        tty->row += (int32_t)y;
    }
    else
    {
        tty->column = (int32_t)x;
        tty->row = (int32_t)y;
    }

    // Set within limits
    if(tty->column < 0) tty->column = 0;
    else if(tty->column >= tty->width) tty->column = tty->width - 1;

    if(tty->row < 0) tty->row = 0;
    else if(tty->row >= tty->height) tty->row = tty->height - 1;
}

void tty_set_colours(tty_dev_t* tty, uint8_t fg, uint8_t bg)
{
    tty->default_colour.fg_colour = fg;
    tty->default_colour.bg_colour = bg;
}

void tty_set_palette(tty_dev_t* tty, uint32_t* palette, bool refresh)
{
    tty->current_palette = palette;
    tty->refresh_back = refresh;
}

void tty_restore_palette(tty_dev_t* tty, bool refresh)
{
    tty->current_palette = tty->default_palette;
    tty->refresh_back = refresh;
}

void tty_select_active(tty_dev_t* tty)
{
    active_tty = tty;
}

tty_dev_t* tty_get_active()
{
    return active_tty;
}

void tty_reshow_fb(tty_dev_t* tty, void* fb_base, uint16_t x, uint16_t y)
{
    mutex_acquire(tty->mutex);

    int printed_chars = 0;
    int i = 0;

    // Draw from the beginning if it was a full redraw
    if(tty->refresh_back)
        i = 0;
    else
        i = tty->last_idx;

    for(; i < tty->current_idx || (i < tty->width * tty->height && tty->just_scrolled); i++)
    {
        int index = (i + tty->display_base) % tty->buffer_size;

        if(tty->buffer_base[index].actual_char == 0 || tty->buffer_base[index].actual_char == '\t')
        {
            // Skip null and tab bytes
            continue;
        }
        else if(tty->buffer_base[index].actual_char == '\n')
        {
            // Move onto next line
            i += tty->width - (i % tty->width) - 1;
            continue;
        }

        fb_fill_putchar(fb_base,
                    ((i % tty->width) << 3) + x,
                    ((i / tty->width) << 4) + y,
                    tty->buffer_base[index].actual_char,
                    tty->current_palette[tty->buffer_base[index].colour.fg_colour],
                    tty->current_palette[tty->buffer_base[index].colour.bg_colour]);
        printed_chars++;
    }

    for(i = tty->current_idx; i < tty->last_idx; i++)
    {
        fb_fill_putchar(fb_base,
                    (i % tty->width) << 3,
                    (i / tty->width) << 4,
                    EMPTY_CHAR,
                    tty->current_palette[tty->buffer_base[i].colour.fg_colour],
                    tty->current_palette[tty->buffer_base[i].colour.bg_colour]);
        printed_chars++;
    }

    mutex_release(tty->mutex);
}

void tty_reshow_vga(tty_dev_t* tty, void* vga_base, uint16_t x, uint16_t y)
{
    // EGA Textmode console
    uint16_t* console_base = vga_base;

    int i = 0;

    if(tty->is_dirty)
        i = 0;
    else
        i = tty->last_idx;

    for(; i < tty->current_idx; i++)
    {
        unsigned char pchar = tty->buffer_base[i].actual_char;
        if( tty->buffer_base[i].actual_char == '\t' ||
            tty->buffer_base[i].actual_char == 0)
        {
            pchar = EMPTY_CHAR;
        }
        else if(tty->buffer_base[i].actual_char == '\n')
        {
            // Skip null bytes
            i -= i % tty->width - (tty->width - 1);
            continue;
        }
        console_base[i] = (tty->buffer_base[i].colour.bg_colour << 12) | (tty->buffer_base[i].colour.fg_colour << 8) | pchar;
    }

    if(!tty->refresh_back)
        return;

    // Erase extra charachters
    for(int i = tty->current_idx; i < tty->last_idx; i++)
        console_base[i] = (tty->buffer_base[i].colour.bg_colour << 12) | (tty->buffer_base[i].colour.fg_colour << 8) | EMPTY_CHAR;
}

// TODO: Test if it works
void tty_reshow_serial(tty_dev_t* tty)
{
    for(uint16_t uart_base = tty->last_idx; uart_base < tty->current_idx; uart_base++)
    {
        if( tty->buffer_base[uart_base].actual_char < ' ' &&
            tty->buffer_base[uart_base].actual_char != '\n' &&
            tty->buffer_base[uart_base].actual_char != '\r') continue;
        if(uart_base >= TTY_SIZE)
        {
            uart_base = TTY_SIZE - tty->width;
        }
        else if(tty->buffer_base[uart_base].actual_char == '\n')
        {
            uart_writec('\n');
            uart_writec('\r');
        }
        else {
            uart_writec(tty->buffer_base[uart_base].actual_char);
        }
    }

    if(!tty->refresh_back)
        return;

    for(int i = tty->current_idx; i < tty->last_idx; i++)
    {
        uart_writec('\b');
        uart_writec('\0');
        uart_writec('\b');
    }
}


bool tty_is_dirty(tty_dev_t* tty)
{
    return tty->is_dirty;
}

void tty_make_clean(tty_dev_t* tty)
{
    tty->just_scrolled = false;
    tty->is_dirty = false;
    tty->refresh_back = false;
    tty->last_idx = tty->current_idx;
}

void tty_clear(tty_dev_t* tty, bool clear_screen)
{
    mutex_acquire(tty->mutex);

    tty->column = 0;
    tty->row = 0;

    if(clear_screen)
    {
        tty->last_idx = 0;
        tty->current_idx = 0;
        tty->draw_base = 0;
        tty->display_base = 0;
        
        memset(tty->buffer_base, 0, tty->width * tty->height * 2);
        tty->refresh_back = true;
    }

    tty->scrollback_limit = tty->draw_base;

    mutex_release(tty->mutex);
}
