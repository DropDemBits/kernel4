#include <types.h>

#ifndef __FB_H__
#define __FB_H__

extern linear_addr_t* get_fb_address();
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

#endif /* __FB_H__ */
