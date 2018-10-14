#include <common/types.h>
#include <stack_state.h>

#ifndef __IDT_H__
#define __IDT_H__ 1

#define IRQ_BASE 32

struct isr_handler
{
    isr_t function;
    void* parameters;
};

void isr_add_handler(uint8_t index, isr_t function, void* parameters);

#endif
