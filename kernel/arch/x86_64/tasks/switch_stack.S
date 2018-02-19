.section .text

.global switch_stack
.global initialize_thread

/**
 * Switches the stack to the new pointer and saves the old into the thread's
 * register state. Old rsp will not be saved if rdi is KNULL, and the stack will
 * not change if rsi is KNULL.
 *
 * Parameters:
 * RDI: Pointer to new thread's register state
 * RSI: Pointer to old thread's register state
 */
switch_stack:
	push %rbp
	push %rbx
	push %r12
	push %r13
	push %r14
	push %r15

	movq $0xDEAD000000000000, %rax
	cmpq %rax, %rsi
	jz 1f
	movq %rsp, 0(%rsi)
1:
	cmpq %rax, %rdi
	jz 2f
	movabsq $tss_begin, %rdx
	movq 0(%rdi), %rdi
	movq %rdi, 4(%rdx)
	movq %rdi, %rsp
2:
	popq %r15
	popq %r14
	popq %r13
	popq %r12
	popq %rbx
	popq %rbp
	xorq %rax, %rax
	ret

/**
 * Entry point of a new thread.
 * Initializes the thread context and does an apropriate iret.
 *
 * Parameters:
 * RBP: pointer to thread block
 * Current stack: properly formatted iret structure
 */
initialize_thread:
	# Set thread state to running
	movl $1, 24(%rbp)

	# Cleanup registers
	xor %rbp, %rbp
	xor %r11, %r11
	xor %r10, %r10
	xor %r9, %r9
	xor %r8, %r8
	xor %rdi, %rdi
	xor %rsi, %rsi
	xor %rax, %rax
	xor %rdx, %rdx
	xor %rcx, %rcx

	iretq
