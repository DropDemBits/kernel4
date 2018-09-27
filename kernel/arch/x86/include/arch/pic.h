#include <common/types.h>

#ifndef __PIC_H__
#define __PIC_H__

void pic_setup();
void pic_disable();
void pic_mask(uint8_t irq);
void pic_unmask(uint8_t irq);
bool pic_check_spurious(uint8_t irq);
void pic_eoi(uint8_t irq);
uint16_t pic_read_irr();
struct ic_dev* pic_get_dev();

#endif /* __PIC_H__ */
