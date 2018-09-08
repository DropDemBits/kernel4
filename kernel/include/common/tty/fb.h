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

#include <common/types.h>

#ifndef __FB_H__
#define __FB_H__

extern void* get_fb_address();
extern struct framebuffer_info fb_info;

enum fb_type
{
    TYPE_INDEXED = 0,
    TYPE_RGB = 1,
    TYPE_EGA_TEXT = 2,
};

struct framebuffer_info
{
    uint64_t base_addr;
    uint32_t pitch;
    uint32_t width;
    uint32_t height;
    uint8_t bits_pp;
    uint8_t bytes_pp;
    enum fb_type type;
    // Palette Info (VALID if type == TYPE_INDEXED)
    uint16_t palette_size;
    uint64_t palette_addr;
    // Colour Info (Valid if type == TYPE_RGB)
    uint8_t red_position;
    uint8_t red_mask_size;
    uint8_t green_position;
    uint8_t green_mask_size;
    uint8_t blue_position;
    uint8_t blue_mask_size;
};

void fb_init();
void fb_plotpixel(void* vram, uint32_t x, uint32_t y, uint32_t colour);
void fb_fillrect(void* vram, uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint32_t colour);
void fb_puts(void* vram, uint16_t x, uint16_t y, const char* str);
void fb_putchar(void* vram, uint16_t x, uint16_t y, const char c, uint32_t colour);
void fb_fill_putchar(void* vram, uint16_t x, uint16_t y, const char c, uint32_t colour, uint32_t bg_colour);
void fb_clear();

#endif /* __FB_H__ */
