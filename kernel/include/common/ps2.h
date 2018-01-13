#include <types.h>

#ifndef __PS2_H__
#define __PS2_H__

extern void ps2_controller_writeb(uint8_t data);
extern void ps2_device_writeb(uint8_t data);
extern uint8_t ps2_read_status();
extern uint8_t ps2_read_data();
extern uint8_t* ps2_device_irqs();

void ps2_init();
void ps2_handle_device(int device, isr_t handler);
uint8_t ps2_device_read(int device, bool wait_for);
void ps2_device_write(int device, bool wait_for, uint8_t data);

#endif /* __PS2_H__ */
