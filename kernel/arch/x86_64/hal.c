/**
 * Copyright (C) 2018 DropDemBits
 * 
 * This file is part of Kernel4.
 * 
 * Kernel4 is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * Kernel4 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with Kernel4.  If not, see <http://www.gnu.org/licenses/>.
 * 
 */

#include <stdio.h>

#include <common/sched.h>
#include <common/hal.h>
#include <x86_64/pic.h>
#include <x86_64/idt.h>
#include <x86_64/stack_state.h>

extern void irq_common(struct intr_stack *frame);

static bool use_apic = false;
static size_t native_flags = 0;
static struct heap_info heap_context = {
	.base=0xFFFF900000000000, .length=0x0000100000000000
};

static bool apic_exists()
{
	uint32_t edx;
	asm volatile("cpuid":"=d"(edx):"a"(1));
	return (edx >> 9) & 0x1;
}

void hal_init()
{
	// Check for APIC
	if(apic_exists())
	{
		// TODO: Enable & init apic
		//outb(PIC1_DATA, 0xFF);
		//outb(PIC2_DATA, 0xFF);
	}
	pic_init();

	for(int i = 0; i < 16; i++)
		irq_add_handler(i, irq_common);

	if(!use_apic)
	{
		for(int i = 0; i < 16; i++) pic_eoi(i);
	}
	timer_config_counter(0, 1000, TM_MODE_INTERVAL);

	pic_unmask(0);
	pic_mask(8);

}

void hal_enable_interrupts()
{
	asm volatile("sti");
}

void hal_disable_interrupts()
{
	asm volatile("cli");
}

void hal_save_interrupts()
{
	asm volatile("pushfq\n\tpopq %%rax":"=a"(native_flags));
	hal_disable_interrupts();
}

void hal_restore_interrupts()
{
	asm volatile("push %%rax\n\tpopfq"::"a"(native_flags));
}

inline void busy_wait()
{
	asm volatile("pause");
}

void timer_config_counter(uint16_t id, uint16_t frequency, uint8_t mode)
{
	pit_init_counter(id, frequency, mode);
}

void timer_reset_counter(uint16_t id)
{
	pit_reset_counter(id);
}

void timer_set_counter(uint16_t id, uint16_t frequency)
{
	pit_set_counter(id, frequency);
}

void ic_mask(uint16_t irq)
{
	pic_mask(irq);
}

void ic_unmask(uint16_t irq)
{
	pic_unmask(irq);
}

void ic_check_spurious(uint16_t irq)
{
	pic_check_spurious(irq);
}

void ic_eoi(uint16_t irq)
{
	pic_eoi(irq);
}

void irq_add_handler(uint16_t irq, isr_t handler)
{
	isr_add_handler(irq + 32, handler);
}

struct heap_info* get_heap_info()
{
	return &heap_context;
}

void dump_registers(struct intr_stack *stack)
{
	printf("***BEGIN REGISTER DUMP***\n");
	printf("RAX: %#p, RBX: %#p, RCX: %#p, RDX: %#p\n", stack->rax, stack->rbx, stack->rcx, stack->rdx);
    printf("RSI: %#p, RDI: %#p, RSP: %#p, RBP: %#p\n", stack->rsi, stack->rdi, stack->rsp, stack->rbp);
    printf("RIP: %#p\n", stack->rip);
    printf("Error code: %x\n", stack->err_code);
    thread_t* at = sched_active_thread();
    printf("Current Thread: %#p\n", at);
    if(at != KNULL)
    {
        printf("\tID: %d\n", at->tid);
        printf("\tRegisters: %#p\n", at->register_state);
        printf("\tPriority: %d\n", at->priority);

        if(at->register_state != KNULL)
            printf("\tKRSP: %#p, RSP: %#p", at->register_state->kernel_rsp, at->register_state->rsp);
    } else
	{
		puts("(idle_thread)");
	}
}
