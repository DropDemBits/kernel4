#include <types.h>

#ifndef __IDT_H__
#define __IDT_H__ 1

typedef struct isr_func
{
	isr_t pointer;
	struct isr_func *next;
} __attribute__((__packed__)) isr_function_t;
void isr_add_handler(uint8_t index, isr_t function);

#endif
