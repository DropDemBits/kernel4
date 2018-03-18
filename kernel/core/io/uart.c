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

#include <common/uart.h>

extern void uart_writeb(uint8_t device, uint8_t offset, uint8_t data);
extern uint8_t uart_readb(uint8_t device, uint8_t offset);

enum
{
	COM1 = 0x0,
	COM2 = 0x1,
	COM3 = 0x2,
	COM4 = 0x3,
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

uint8_t default_device = 0xFF;

static uint8_t init_com_device(uint8_t device, uint16_t divisor)
{
	// Test if device exists
	uart_writeb(device, SCRATCH, 0xDA);
	if(uart_readb(device, SCRATCH) != 0xDA)	return 0;

	// Initialize Device
	uart_writeb(device, INTR_CONTROL, 0x00);    // Disable all interrupts
	uart_writeb(device, LINE_CONTROL, 0x80);    // Enable DLAB (set baud rate divisor)
	uart_writeb(device, 0, (divisor >> 0) & 0xFF);    // Set divisor to low byte
	uart_writeb(device, 1, (divisor >> 8) & 0xFF);    //                hi byte
	uart_writeb(device, LINE_CONTROL, 0x03);    // 8 bits, no parity, one stop bit
	uart_writeb(device, FIFO_CONTROL, 0xC7);    // Enable & clear FIFO, w/ 14-byte limit
	uart_writeb(device, MODEM_CONTROL, 0x0B);   // Enable interrupts
	return 1;
}

static void wait_read(uint8_t device)
{
	while((uart_readb(device, LINE_STATUS) & 0b00000001) == 0x0);// asm("pause");
}

static void wait_write(uint8_t device)
{
	while((uart_readb(device, LINE_STATUS) & 0b00100000) == 0x0);// asm("pause");
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
	if(default_device == 0xFF) return;
	wait_write(default_device);
	uart_writeb(default_device, DATA, c);
}

void uart_writestr(const char* str, size_t length)
{
	if(default_device == 0xFF) return;
	wait_write(default_device);
	for(size_t i = 0; i < length; i++)
	{
		uart_writeb(default_device, DATA, str[i]);
		if(uart_readb(default_device, LINE_STATUS) & 0b00000010)
		{
			// We have filled our fifo buffer, wait until available
			wait_write(default_device);
		}
	}
}

uint8_t uart_read()
{
	wait_read(default_device);
	return uart_readb(default_device, DATA);
}
