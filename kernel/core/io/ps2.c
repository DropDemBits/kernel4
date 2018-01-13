#include <ps2.h>
#include <hal.h>
#include <stdio.h>

#define STATUS_READREADY 0x01
#define STATUS_WRITEREADY 0x02

/*
   External methods

   extern void ps2_controller_writeb(uint8_t data);
   extern void ps2_device_writeb(uint8_t data);
   extern uint8_t ps2_read_status();
   extern uint8_t ps2_read_data();
   extern uint8_t* ps2_device_irqs();
 */

uint8_t valid_devices[2];

static void send_controller_command(uint8_t command)
{
	while((ps2_read_status() & STATUS_WRITEREADY) == 1) busy_wait();
	ps2_controller_writeb(command);
}

static void wait_write_data(uint8_t data)
{
	while((ps2_read_status() & STATUS_WRITEREADY) == 1) busy_wait();
	ps2_device_writeb(data);
}

static uint8_t wait_read_data()
{
	while((ps2_read_status() & STATUS_READREADY) == 0) busy_wait();
	return ps2_read_data();
}

void ps2_init()
{
	uint8_t usable_bitmask = 0b01;

	// Disable devices
	send_controller_command(0xAD); // Device 1
	send_controller_command(0xA7); // Device 2

	// Clear output buffer
	while(ps2_read_status() & 0x1) ps2_read_data();

	// Set configuration byte
	send_controller_command(0x20);
	uint8_t cfg_byte = wait_read_data();
	cfg_byte &= 0b10111100;
	send_controller_command(0x60);
	wait_write_data(cfg_byte);
	if(cfg_byte & 0x20) usable_bitmask |= 0b10;

	// Check for poor PS/2 controller emulation
	send_controller_command(0x20);
	if(wait_read_data() != cfg_byte)
	{
		send_controller_command(0xAE);
		send_controller_command(0xA8);
		puts("PS/2 Buggy controller detected");
		return;
	}

	// Perform self test
	send_controller_command(0xAA);
	if(wait_read_data() == 0xFC)
	{
		puts("PS/2 Self test failed");
		return;
	}

	// Test ports
	send_controller_command(0xAB);
	uint8_t retval = wait_read_data();
	if(retval != 0x00)
	{
		usable_bitmask &= ~0b01;
		printf("PS/2 Unable to use device 1 (code %#x)\n", retval);
	}

	if(usable_bitmask & 0b10)
	{
		send_controller_command(0xA9);
		uint8_t retval = wait_read_data();
		if(retval != 0x00)
		{
			usable_bitmask &= ~0b10;
			printf("PS/2 Unable to use device 2 (code %#x)\n", retval);
		}
	}
	
	if(usable_bitmask == 0)
	{
		puts("PS/2 No usable devices");
		return;
	}

	// Enable & reset devices
	if(usable_bitmask & 0b01)
	{
		send_controller_command(0xAE);
		ps2_device_write(0, true, 0xFF);
		if(ps2_device_read(0, true) == 0xFC) usable_bitmask &= ~0b01;
		else
			valid_devices[0] = 1;
		if(valid_devices[0]) puts("PS/2 Device exists on port 1.");
	}

	if(usable_bitmask & 0b10)
	{
		send_controller_command(0xA8);
		ps2_device_write(1, true, 0xFF);
		if(ps2_device_read(1, true) == 0xFC) usable_bitmask &= ~0b10;
		else
			valid_devices[1] = 1;
		if(valid_devices[1]) puts("PS/2 Device exists on port 2.");
	}

	// Commit changes
	send_controller_command(0x20);
	cfg_byte = wait_read_data();
	cfg_byte |= usable_bitmask;
	send_controller_command(0x60);
	wait_write_data(cfg_byte);
}

void ps2_handle_device(int device, isr_t handler)
{
	if(device >= 2 || !valid_devices[device]) return;
	irq_add_handler(ps2_device_irqs()[device], handler);
}

uint8_t ps2_device_read(int device, bool wait_for)
{
	if(device >= 2 || !valid_devices[device]) return 0x00;

	if(wait_for) return wait_read_data();
	else return ps2_read_data();
}

void ps2_device_write(int device, bool wait_for, uint8_t data)
{
	if(device >= 2 || !valid_devices[device]) return;
	else if(device == 1) ps2_controller_writeb(0xD4);

	if(wait_for) wait_write_data(data);
	else ps2_device_writeb(data);
}
