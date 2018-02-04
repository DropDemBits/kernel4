#include <ps2.h>
#include <hal.h>
#include <stdio.h>

#define STATUS_READREADY 0x01
#define STATUS_WRITEREADY 0x02
/* Corresponds to 5ms */
#define DEFAULT_TIMEOUT 5

#define WAIT_TIMEOUT_CHECK(condition, timeout, timedout) \
do { \
	int cval = (timeout); \
	do { \
		ps2_wait(); \
	} while((condition) && --cval); \
	if(!cval) (timedout) = true; \
} while(0); \

#define WAIT_TIMEOUT(condition, timeout) \
do { \
	bool unused = false; \
	WAIT_TIMEOUT_CHECK(condition, timeout, unused) \
} while(0); \

/*
   External methods

   extern void ps2_wait();
   extern void ps2_controller_writeb(uint8_t data);
   extern void ps2_device_writeb(uint8_t data);
   extern uint8_t ps2_read_status();
   extern uint8_t ps2_read_data();
   extern uint8_t* ps2_device_irqs();
 */

extern void ps2kbd_init(int device);
extern void atkbd_init(int device);
//extern void ps2mouse_init(int device);

bool controller_exists = true;
static const char* type2name[] = {
	"None",
	"AT Keyboard",
	"MF2 Keyboard",
	"Translated MF2 Keyboard",
	"2-button Mouse",
	"3-button Mouse",
	"5-button Mouse"
};
struct ps2_device devices[2];

static void send_controller_command(uint8_t command)
{
	WAIT_TIMEOUT((ps2_read_status() & STATUS_WRITEREADY), DEFAULT_TIMEOUT)
	ps2_controller_writeb(command);
}

static void send_dev_command(int device, uint8_t command)
{
	ps2_device_write(device, true, command);
	if(ps2_device_read(device, true) != 0xFA) return;
}

static void wait_write_data(uint8_t data)
{
	WAIT_TIMEOUT((ps2_read_status() & STATUS_WRITEREADY), DEFAULT_TIMEOUT)
	ps2_device_writeb(data);
}

static uint8_t wait_read_data()
{
	WAIT_TIMEOUT((ps2_read_status() & STATUS_READREADY) == 0, DEFAULT_TIMEOUT)
	return ps2_read_data();
}

static uint8_t wait_read_data_timeout()
{
	bool timedout = false;
	WAIT_TIMEOUT_CHECK((ps2_read_status() & STATUS_READREADY) == 0, DEFAULT_TIMEOUT, timedout)
	if(timedout)
		return 0xFF;
	return ps2_read_data();
}

static void controller_init()
{
	// Temporary until we can read ACPI Tables
	if(ps2_read_status() == 0xFF)
	{
		controller_exists = false;
		return;
	}

	uint8_t usable_bitmask = 0b01;
	bool modify_cfg = true;

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
		puts("PS/2 Unable to change config byte");
		modify_cfg = false;
		goto device_init;
	}

	// Perform self test
	send_controller_command(0xAA);
	if(wait_read_data() == 0xFC)
	{
		puts("PS/2 Self test failed");
		return;
	}

	// Test ports
	device_init:
	send_controller_command(0xAB);
	uint8_t retval = wait_read_data();
	if(retval != 0x00)
	{
		usable_bitmask &= ~0b01;
		printf("PS/2 Unable to use device on port 1 (code %#x)\n", retval);
	}

	if(usable_bitmask & 0b10)
	{
		send_controller_command(0xA9);
		uint8_t retval = wait_read_data();
		if(retval != 0x00)
		{
			usable_bitmask &= ~0b10;
			printf("PS/2 Unable to use device on port 2 (code %#x)\n", retval);
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
		ps2_device_write(0, true, 0xFF);
		if(ps2_device_read(0, true) == 0xFC) usable_bitmask &= ~0b01;
		else
			devices[0].present = 1;
		if(devices[0].present) puts("PS/2 Device exists on port 1.");
	}

	if(usable_bitmask & 0b10)
	{
		ps2_device_write(1, true, 0xFF);
		if(ps2_device_read(1, true) == 0xFC) usable_bitmask &= ~0b10;
		else
			devices[1].present = 1;
		if(devices[1].present) puts("PS/2 Device exists on port 2.");
	}

	if(!modify_cfg) return;

	// Commit changes
	send_controller_command(0x20);
	cfg_byte = wait_read_data();
	cfg_byte |= usable_bitmask;
	send_controller_command(0x60);
	wait_write_data(cfg_byte);

}

static void detect_device(int device)
{
	// Disable Scanning
	send_dev_command(device, 0xF5);

	if(device >= 2 || device < 0 || !devices[device].present)
	{
		devices[device].type = TYPE_NONE;
		return;
	}

	uint16_t first_byte = 0xFF, second_byte = 0xFF;
	printf("Identifying device on port %d\n", device + 1);
	send_dev_command(device, 0xF2);
	first_byte = wait_read_data_timeout();
	second_byte = wait_read_data_timeout();

	if(first_byte == 0xFA && second_byte == 0xFF) devices[device].type = TYPE_AT_KBD;
	else if(first_byte == 0x00 && second_byte == 0xFF) devices[device].type = TYPE_2B_MOUSE;
	else if(first_byte == 0x03 && second_byte == 0xFF) devices[device].type = TYPE_3B_MOUSE;
	else if(first_byte == 0x04 && second_byte == 0xFF) devices[device].type = TYPE_5B_MOUSE;
	else if(first_byte == 0xAB && second_byte == 0x41) devices[device].type = TYPE_MF2_KBD_TRANS;
	else if(first_byte == 0xAB && second_byte == 0xC1) devices[device].type = TYPE_MF2_KBD_TRANS;
	else if(first_byte == 0xAB && second_byte == 0x83) devices[device].type = TYPE_MF2_KBD;
	else printf("Identified unknown device on port %d: %#x %#x\n", device+1, first_byte, second_byte);

	// The following combination isn't possible
	if((devices[device].type == TYPE_MF2_KBD_TRANS || devices[device].type == TYPE_AT_KBD) &&
		device == 1)
	{
		devices[device].type = TYPE_NONE;
		devices[device].present = 0;
	}

	if(device == 0) send_controller_command(0xAE);
	else if(device == 1) send_controller_command(0xA8);
}

void ps2_init()
{
	controller_init();
	detect_device(0);
	detect_device(1);

	// Initialize devices
	for(int active_device = 0; active_device < 2; active_device++)
	{
		if(!devices[active_device].present || !devices[active_device].type) continue;

		if(devices[active_device].type == TYPE_MF2_KBD)
		{
			printf("Initialising MF2 keyboard\n");
			ps2kbd_init(active_device);
		} else if(devices[active_device].type == TYPE_MF2_KBD_TRANS ||
				devices[active_device].type == TYPE_AT_KBD)
		{
			printf("Initialising AT/Translated MF2 keyboard\n");
			atkbd_init(active_device);
		}
		else
			printf("Device on port %d not initialized (%s)\n",
				active_device+1,
				type2name[devices[active_device].type]);
	}
}

void ps2_handle_device(int device, isr_t handler)
{
	if(device >= 2 || device < 0 || !devices[device].present) return;
	irq_add_handler(ps2_device_irqs()[device], handler);
}

uint8_t ps2_device_read(int device, bool wait_for)
{
	if(device >= 2 || device < 0 || !devices[device].present) return 0x00;

	if(wait_for) return wait_read_data();
	else return ps2_read_data();
}

void ps2_device_write(int device, bool wait_for, uint8_t data)
{
	if(device >= 2 || device < 0 || !devices[device].present) return;
	else if(device == 1) ps2_controller_writeb(0xD4);

	if(wait_for) wait_write_data(data);
	else ps2_device_writeb(data);
}

enum ps2_devtype ps2_device_get_type(int device)
{
	if(device >= 2 || device < 0) return TYPE_NONE;
	return devices[device].type;
}

void ps2_controller_set_exist(bool exist_state)
{
	controller_exists = exist_state;
}

bool ps2_controller_exists()
{
	return controller_exists;
}
