#include <ps2.h>
#include <io.h>

uint8_t irqs[] = {1, 12};

void ps2_controller_writeb(uint8_t data)
{
	outb(0x64, data);
}

void ps2_device_writeb(uint8_t data)
{
	outb(0x60, data);
}

uint8_t ps2_read_status()
{
	return inb(0x64);
}

uint8_t ps2_read_data()
{
	return inb(0x60);
}

uint8_t* ps2_device_irqs()
{
	return irqs;
}

void ps2_wait()
{
	io_wait();
	asm("hlt");
}
