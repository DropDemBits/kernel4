#include <uart.h>
#include <io.h>

enum
{
	COM1 = 0x3F8,
	COM2 = 0x2F8,
	COM3 = 0x3E8,
	COM4 = 0x2E8,
	// IO Port offsets
	DATA = 0,
	INTR_CONTROL = 1,
	INTR_ID = 2,
	FIFO_CONTROL = 2,
	LINE_CONTROL = 3,
	MODEM_CONTROL = 4,
	LINE_STATUS = 5,
	MODEM_STATUS = 6,
	SCRATCH = 7,
};

uint16_t default_device = 0x000;

static uint8_t init_com_device(uint16_t port, uint16_t divisor)
{
	// Test if device exists
	outb(port + SCRATCH, 0xDA);
	if(inb(port + SCRATCH) != 0xDA)	return 0;

	// Initialize Device (modified from wiki.osdev.org/UART)
	outb(port + INTR_CONTROL, 0x00);    // Disable all interrupts
	outb(port + LINE_CONTROL, 0x80);    // Enable DLAB (set baud rate divisor)
	outb(port + 0, (divisor >> 0) & 0xFF);    // Set divisor to low byte
	outb(port + 1, (divisor >> 8) & 0xFF);    //                hi byte
	outb(port + LINE_CONTROL, 0x03);    // 8 bits, no parity, one stop bit
	outb(port + FIFO_CONTROL, 0xC7);    // Enable & clear FIFO, w/ 14-byte limit
	outb(port + MODEM_CONTROL, 0x0B);   // Enable interrupts
	return 1;
}

static void wait_read(uint16_t port)
{
	while((inb(port + LINE_STATUS) & 0b00000001) == 0x0) asm("pause");
}

static void wait_write(uint16_t port)
{
	while((inb(port + LINE_STATUS) & 0b00100000) == 0x0) asm("pause");
}

void uart_init()
{
	// 0x0003 = 0x38400 baud
	if(init_com_device(COM4, 0x03)) default_device = COM4;
	if(init_com_device(COM3, 0x03)) default_device = COM3;
	if(init_com_device(COM2, 0x03)) default_device = COM2;
	if(init_com_device(COM1, 0x03)) default_device = COM1;
}

void uart_writec(char c)
{
	if(default_device == 0x0000) return;
	wait_write(default_device);
	outb(default_device, c);
}

void uart_writestr(const char* str, size_t length)
{
	if(default_device == 0x0000) return;
	wait_write(default_device);
	for(size_t i = 0; i < length; i++)
	{
		outb(default_device, str[i]);
		if(inb(default_device + LINE_STATUS) & 0b00000010)
		{
			// We have filled our fifo buffer, wait until available
			wait_write(default_device);
		}
	}
}

uint8_t uart_read()
{
	wait_read(default_device);
	return inb(default_device);
}
