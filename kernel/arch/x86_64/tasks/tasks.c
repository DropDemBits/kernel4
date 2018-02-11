#include <tasks.h>
#include <mm.h>

extern void initialize_thread();

/****SUPER TEMPORARY ADDRESS ALLOCATION****/
static uint64_t alloc_base = 0x00007FFFFFFFF000;

uint64_t alloc_address()
{
	uint64_t retval = alloc_base;
	alloc_base -= 0x5000; // 16KiB store, 4KiB guard page
	return retval;
}

void init_register_state(thread_t *thread, uint64_t *entry_point)
{
	uint64_t *kernel_stack = (uint64_t*) alloc_address();
	struct thread_registers *registers = thread->register_state;

	registers->rsp = alloc_address();

	for(uint64_t i = 0; i < 4; i++)
	{
		mmu_map(kernel_stack - (i << 9));
		mmu_map((uint64_t*)(registers->rsp - (i << 12)));
	}

	// IRET structure (Right now we only have kernel space threads)
	*(kernel_stack--) = 0x00; // SS
	*(kernel_stack--) = registers->rsp; // RSP
	*(kernel_stack--) = 0x0202; // RFLAGS
	*(kernel_stack--) = 0x08; // CS
	*(kernel_stack--) = (uint64_t) entry_point; // RIP

	// initialize_thread stack
	*(kernel_stack--) = (uint64_t) initialize_thread; // Return address
	*(kernel_stack--) = (uint64_t) thread; // RBP
	kernel_stack -= 4; // R15-12, RBX
	registers->kernel_rsp = kernel_stack;

	// General registers
	registers->rip = (uint64_t) entry_point;
}

void cleanup_register_state(thread_t *thread)
{
	struct thread_registers *registers = thread->register_state;

	for(uint64_t i = 0; i < 4; i++)
	{
		mmu_unmap((uint64_t*)((registers->kernel_rsp & ~0xFFF) - (i << 12)));
		mmu_unmap((uint64_t*)((registers->rsp & ~0xFFF) - (i << 12)));
	}
}
