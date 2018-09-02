#include <common/types.h>

#ifndef __IDT_H__
#define __IDT_H__ 1

#define IRQ_BASE 32

void isr_add_handler(uint8_t index, isr_t function);

#endif
