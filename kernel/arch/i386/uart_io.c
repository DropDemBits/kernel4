#include <io.h>

enum
{
	COM1 = 0x3F8,
	COM2 = 0x2F8,
	COM3 = 0x3E8,
	COM4 = 0x2E8,
};
static uint16_t ports[] = {COM1, COM2, COM3, COM4};

void uart_writeb(uint8_t device, uint8_t offset, uint8_t data)
{
	if(device >= 4) return;
	outb(ports[device] + offset, data);
}

uint8_t uart_readb(uint8_t device, uint8_t offset)
{
	return inb(ports[device] + offset);
}
