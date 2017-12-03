#include <types.h>

#ifndef __IDT_H__
#define __IDT_H__ 1

/* void* is optional stack frame */
typedef uint8_t(*isr_t)(void*);

typedef struct
{
	isr_t pointer;
} __attribute__((__packed__)) isr_function_t;

void isr_add_handler(uint8_t index, isr_t function);

#endif
