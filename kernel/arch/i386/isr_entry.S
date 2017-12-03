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
	push %ebp
	push %esi
	push %edi
	push %ebx

	movl %cr2, %ebx
	push %ebx

	push %ecx
	push %edx
	push %eax

	# Call isr_common
	movl %esp, %eax
	push %eax
	call isr_common
	addl $4, %esp

	# Restore registers
	popl %eax
	popl %edx
	popl %ecx

	popl %ebx
	movl %ebx, %cr2

	popl %ebx
	popl %edi
	popl %esi
	popl %ebp

	addl $8, %esp

	iret
