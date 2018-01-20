#include <fb.h>
#include <tty.h>

extern unsigned char font_bits[];

struct framebuffer_info fb_info;

void fb_init()
{
	// TODO: If this is an indexed palette framebuffer, calculate a colour
	// TODO: quantization table.
	if(fb_info.bits_pp) tty_prints("fb_info.bits_pp is not 0\n");
	if(fb_info.bytes_pp) tty_prints("fb_info.bytes_pp is not 0\n");
	if(fb_info.pitch) tty_prints("fb_info.pitch is not 0\n");
}

void fb_puts(void* vram, uint16_t x, uint16_t y, const char* str)
{
	while(*str)
	{
		fb_putchar(vram, x, y, *str, 0xFFFFFFFF);
		x+=8;
		str++;
	}
}

void fb_putchar(void* vram, uint16_t x, uint16_t y, const char c, uint32_t colour)
{
	uint8_t *font_base = &font_bits[(size_t)(c)];

	for(uint8_t h = 0; h < 16; h++)
	{
		for(uint8_t w = 0; w < 8; w++)
		{
			if((*font_base >> w) & 0x1)
			{
				fb_plotpixel(vram, x+w, y+h, colour);
			}
		}
		font_base += 128;
	}
}
void fb_fill_putchar(void* vram, uint16_t x, uint16_t y, const char c, uint32_t colour, uint32_t bg_colour)
{
	uint8_t *font_base = &font_bits[(size_t)(c)];

	for(uint8_t h = 0; h < 16; h++)
	{
		for(uint8_t w = 0; w < 8; w++)
		{
			if((*font_base >> w) & 0x1)
				fb_plotpixel(vram, x+w, y+h, colour);
			else
				fb_plotpixel(vram, x+w, y+h, bg_colour);
		}
		font_base += 128;
	}
}

void fb_plotpixel(void* vram, uint32_t x, uint32_t y, uint32_t colour)
{
	unsigned where = x*fb_info.bytes_pp + y*fb_info.pitch;
	uint8_t* screen = vram;

	if(fb_info.bits_pp == 16 || fb_info.bits_pp == 15)
	{
		uint16_t word = 0;
		word |= ((colour >> 3) & 0x1F) << 0; // BLUE
		if(fb_info.bits_pp == 15)
		{
			word |= ((colour >> 11) & 0x1F) << 5;  // GREEN (5)
			word |= ((colour >> 19) & 0x1F) << 10; // RED (5)
		}
		else
		{
			word |= ((colour >> 10) & 0x3F) << 5;  // GREEN (6)
			word |= ((colour >> 19) & 0x1F) << 11; // RED (5)
		}
		screen[where + 0] = (word >> 0) & 0xFF;
		screen[where + 1] = (word >> 8) & 0xFF;
	}
	else if(fb_info.bits_pp == 32 || fb_info.bits_pp == 24)
	{
		screen[where + 0] = (colour >> 0) & 0xFF;     // BLUE
		screen[where + 1] = (colour >> 8) & 0xFF;     // GREEN
		screen[where + 2] = (colour >> 16) & 0xFF;    // RED
	}
}

void fb_fillrect(void* vram, uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint32_t colour)
{
	uint8_t* screen = vram;
	uint16_t i, j;
	uint8_t r = (colour >> 0) & 0xFF;
	uint8_t g = (colour >> 8) & 0xFF;
	uint8_t b = (colour >> 16) & 0xFF;
	screen += x*fb_info.bytes_pp + y*fb_info.pitch;

	for (i = 0; i < h; i++)
	{
		for (j = 0; j < w; j++)
		{
			screen[j*fb_info.bytes_pp + 0] = r;
			screen[j*fb_info.bytes_pp + 1] = g;
			screen[j*fb_info.bytes_pp + 2] = b;
		}
		screen += fb_info.pitch;
	}
}
