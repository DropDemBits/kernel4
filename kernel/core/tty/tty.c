#include <tty.h>
#include <fb.h>
#include <mm.h>
#include <uart.h>

#define TTY_SIZE 80*25*1

uint16_t width = 0;
uint16_t height = 0;
uint16_t column = 0;
uint16_t row = 0;
// Used for offset
uint16_t screen_row = 0;
uint16_t uart_base = 0;
tty_colour_t colour = {.fg_colour=0x7,.bg_colour=0x0};
tty_device_t extra_devices[2];
tty_char_t window[TTY_SIZE];
bool background_reshow = false;

static size_t strlen(const char* str)
{
	size_t len = 0;
	while(str[len]) len++;
	return len;
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
	for(int i = 0; i < TTY_SIZE; i++)
	{
		window[i].colour = colour;
		window[i].actual_char = 0x00;
	}
}

void tty_prints(const char* str)
{
	// Print string to tty window
	for(int i = 0; i < strlen(str); i++)
		tty_printchar(str[i]);
	tty_reshow();
}

void tty_printchar(const char c)
{
	// Print char to tty window
	size_t index = column + (row + screen_row) * width;
	window[index].actual_char = c;
	window[index].colour = colour;

	if(c == '\n' || ++column >= width)
	{
		column = 0;
		if(++row >= height)
		{
			row = height - 1;
			tty_scroll();
		}
	}
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
	// Write to UART
	for(; uart_base < (column + (row + screen_row) * width); uart_base++)
	{
		if(	window[uart_base].actual_char < ' ' &&
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
			uart_writec(window[uart_base].actual_char);
		}
	}

	if(extra_devices[VGA_CONSOLE].base != (size_t)KNULL &&
		mmu_is_usable((size_t*)extra_devices[VGA_CONSOLE].base))
	{
		// EGA Textmode console
		uint16_t* console_base = (uint16_t*)extra_devices[VGA_CONSOLE].base;
		for(int i = 0; i < TTY_SIZE; i++)
		{
			if(window[i].actual_char < ' ') continue;
			console_base[i] = (window[i].colour.bg_colour << 12) | (window[i].colour.fg_colour << 8) | window[i].actual_char;
		}
	}

	if(extra_devices[FB_CONSOLE].base != (size_t)KNULL &&
		mmu_is_usable((size_t*)extra_devices[FB_CONSOLE].base))
	{
		for(int i = 0; i < TTY_SIZE; i++)
		{
			if(window[i].actual_char == '\n') continue;
			fb_putchar(extra_devices[FB_CONSOLE].base, (i % width) << 3, (i / width) << 4, window[i].actual_char);
		}
	}
}

/*
 * Adds an output device for the tty to output to
 */
void tty_add_output(enum OutputType type, size_t base)
{
	switch (type) {
		case VGA_CONSOLE:
			extra_devices[VGA_CONSOLE].base = base;
			break;
		case FB_CONSOLE:
			extra_devices[FB_CONSOLE].base = base;
			break;
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
