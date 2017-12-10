.extern isr_common

.macro isr_err_entry int_num
.globl isr\int_num\()_entry
isr\int_num\()_entry:
	push $0
	push $\int_num\()
	jmp isr_entry
.endm

.macro isr_noerr_entry int_num
.globl isr\int_num\()_entry
isr\int_num\()_entry:

	push $\int_num\()
	jmp isr_entry
.endm

.macro irq_entry int_num
.globl irq\int_num\()_entry
irq\int_num\()_entry:
	push $0
	push $\int_num+32\()
	jmp isr_entry
.endm

.macro spur_irq_entry int_num
.globl irq\int_num\()_entry
irq\int_num\()_entry:
	push $0
	push $\int_num+32\()
	jmp isr_entry
.endm

isr_noerr_entry 0
isr_noerr_entry 1
isr_noerr_entry 2
isr_noerr_entry 3
isr_noerr_entry 4
isr_noerr_entry 5
isr_noerr_entry 6
isr_noerr_entry 7
isr_err_entry 8
// RSV 9
isr_err_entry 10
isr_err_entry 11
isr_err_entry 12
isr_err_entry 13
isr_err_entry 14
// RSV 15
isr_noerr_entry 16
isr_err_entry 17
isr_noerr_entry 18
isr_noerr_entry 19
isr_noerr_entry 20
isr_err_entry 21
// RSV 22-28
isr_err_entry 29
isr_err_entry 30
// RSV 31
irq_entry 0
irq_entry 1
irq_entry 2
irq_entry 3
irq_entry 4
irq_entry 5
irq_entry 6
spur_irq_entry 7

irq_entry 8
irq_entry 9
irq_entry 10
irq_entry 11
irq_entry 12
irq_entry 13
irq_entry 14
spur_irq_entry 15

isr_entry:
	# Push other regs
	push %rbp
	push %rbx
	push %r12
	push %r13
	push %r14
	push %r15

	movq %cr2, %rbx
	push %rbx

	push %rcx
	push %rdx
	push %rax
	push %rsi
	push %rdi
	push %r8
	push %r9
	push %r10
	push %r11

	# Call isr_common
	movq %rsp, %rdi
	call isr_common
	addq $8, %rsp

	# Restore registers
	popq %r11
	popq %r10
	popq %r9
	popq %r8
	popq %rdi
	popq %rsi
	popq %rax
	popq %rdx
	popq %rcx

	popq %rbx
	movq %rbx, %cr2

	popq %r15
	popq %r14
	popq %r13
	popq %r12
	popq %rbx
	popq %rbp

	addq $16, %rsp

	iret