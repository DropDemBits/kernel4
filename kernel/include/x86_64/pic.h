#include <x86_64/types.h>

#ifndef __PIC_H__
#define __PIC_H__

void pic_init();
void pic_mask(uint8_t irq);
void pic_unmask(uint8_t irq);
bool pic_check_spurious(uint8_t irq);
void pic_eoi(uint8_t irq);
uint16_t pic_read_irr();

#endif /* __PIC_H__ */
