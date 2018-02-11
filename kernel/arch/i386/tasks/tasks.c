#include <tasks.h>
#include <mm.h>

extern void initialize_thread();

/****SUPER TEMPORARY ADDRESS ALLOCATION****/
static uint32_t alloc_base = 0x7FFFF000;

uint32_t alloc_address()
{
	uint32_t retval = alloc_base;
	alloc_base -= 0x5000; // 16KiB store, 4KiB guard page
	return retval;
}

void init_register_state(thread_t *thread, uint64_t *entry_point)
{
	uint32_t *kernel_stack = (uint32_t*) alloc_address();
	struct thread_registers *registers = thread->register_state;

	registers->esp = alloc_address();

	for(uint32_t i = 0; i < 4; i++)
	{
		mmu_map(kernel_stack - (i << 9));
		mmu_map((uint32_t*)(registers->esp - (i << 12)));
	}

	// IRET structure (Right now we only have kernel space threads)
	*(kernel_stack--) = 0x10; // SS
	*(kernel_stack--) = registers->esp; // ESP
	*(kernel_stack--) = 0x0202; // EFLAGS
	*(kernel_stack--) = 0x08; // CS
	*(kernel_stack--) = (uint32_t) entry_point; // EIP

	// initialize_thread stack
	*(kernel_stack--) = (uint32_t) initialize_thread; // Return address
	*(kernel_stack--) = (uint32_t) thread; // EBP
	kernel_stack -= 2; // EDI, ESI, EBX
	registers->kernel_esp = kernel_stack;

	// General registers
	registers->eip = (uint32_t) entry_point;
}

void cleanup_register_state(thread_t *thread)
{
	struct thread_registers *registers = thread->register_state;
	
	for(uint32_t i = 0; i < 4; i++)
	{
		mmu_unmap((uint32_t*)((registers->kernel_esp & ~0xFFF) - (i << 12)));
		mmu_unmap((uint32_t*)((registers->esp & ~0xFFF) - (i << 12)));
	}
}
