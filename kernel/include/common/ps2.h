#include <types.h>

#ifndef __PS2_H__
#define __PS2_H__

extern void ps2_controller_writeb(uint8_t data);
extern void ps2_device_writeb(uint8_t data);
extern uint8_t ps2_read_status();
extern uint8_t ps2_read_data();
extern uint8_t* ps2_device_irqs();

enum ps2_devtype
{
	TYPE_NONE,
	TYPE_AT_KBD,
	TYPE_MF2_KBD,
	TYPE_MF2_KBD_TRANS,
	TYPE_2B_MOUSE,
	TYPE_3B_MOUSE,
	TYPE_5B_MOUSE,
};

struct ps2_device
{
	uint8_t present;
	enum ps2_devtype type;
};

void ps2_init();
void ps2_handle_device(int device, isr_t handler);
uint8_t ps2_device_read(int device, bool wait_for);
void ps2_device_write(int device, bool wait_for, uint8_t data);
enum ps2_devtype ps2_device_get_type(int device);
void ps2_controller_set_exist(bool exist_state);
bool ps2_controller_exists();

#endif /* __PS2_H__ */
