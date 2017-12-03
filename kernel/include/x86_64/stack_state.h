#include <types.h>

#ifndef __STACK_STATE_H__
#define __STACK_STATE_H__ 1

struct intr_stack {
	// More Registers
	uint64_t r11;
	uint64_t r10;
	uint64_t r9;
	uint64_t r8;
	uint64_t rdi;
	uint64_t rsi;
	uint64_t rax;
	uint64_t rdx;
	uint64_t rcx;
	// Registers
	uint64_t cr2;
	uint64_t r15;
	uint64_t r14;
	uint64_t r13;
	uint64_t r12;
	uint64_t rbx;
	uint64_t rbp;
	// INTR Info
	uint64_t int_num;
	uint64_t err_code;
	// IRET Things
	uint64_t rip;
	uint64_t cs;
	uint64_t rflags;
	uint64_t rsp;
	uint64_t ss;
} __attribute__((__packed__));

struct thread_stack_state
{
	// Preserved
	uint64_t r15;
	uint64_t r14;
	uint64_t r13;
	uint64_t r12;
	uint64_t rbx;
	uint64_t rbp;
	uint64_t rsp;
	uint64_t rip; // call pushed
	// Volatiles
	uint64_t r11;
	uint64_t r10;
	uint64_t r9;
	uint64_t r8;
	uint64_t rdi;
	uint64_t rsi;
	uint64_t rax;
	uint64_t rdx;
	uint64_t rcx;
} __attribute__((__packed__));

#endif /* __STACK_STATE_H__ */
