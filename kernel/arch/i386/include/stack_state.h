#include <types.h>

#ifndef __STACK_STATE_H__
#define __STACK_STATE_H__ 1

struct intr_stack {
    uint32_t ds;
    uint32_t es;
    uint32_t fs;
    uint32_t gs;
    // More Registers
    uint32_t eax;
    uint32_t edx;
    uint32_t ecx;
    // Registers
    uint32_t cr2;
    uint32_t ebx;
    uint32_t edi;
    uint32_t esi;
    uint32_t ebp;
    // INTR Info
    uint32_t int_num;
    uint32_t err_code;
    // IRET Things
    uint32_t eip;
    uint32_t cs;
    uint32_t eflags;
    // Usermode Change (Valid if CPL is 3)
    uint32_t esp;
    uint32_t ss;
} __attribute__((__packed__));

struct stack_state_segs {
    uint32_t gs;
    uint32_t fs;
    uint32_t es;
    uint32_t ds;
    struct intr_stack rest;
} __attribute__((__packed__));

#endif /* __STACK_STATE_H__ */
