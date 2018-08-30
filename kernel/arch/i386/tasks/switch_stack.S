// Copyright (C) 2018 DropDemBits
// 
// This file is part of Kernel4.
// 
// Kernel4 is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// Kernel4 is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with Kernel4.  If not, see <http://www.gnu.org/licenses/>.
// 

.section .text

/**
 * Switches the stack to the new pointer and saves the old into the thread's
 * register state. Old rsp will not be saved if rdi is KNULL, and the stack will
 * not change if rsi is KNULL.
 *
 * Parameters:
 * 4(ESP): Pointer to new thread (ECX)
 * 8(ESP): Pointer to old thread (EDX)
 * 12(ESP): Pointer to new thread's paging context (EAX)
 */
.global switch_stack
switch_stack:
	push %ebp
	push %edi
	push %esi
	push %ebx

	# Put all the things into registers
	movl 20(%esp), %ecx
	movl 24(%esp), %edx
	movl 28(%esp), %eax

	# Update old thread's KESP
	cmpl $0xDEADBEEF, %edx
	jz 1f
	movl %esp, 3*4(%edx)
1:
	// Update Stack (if needed)
	testl $0xDEADBEEF, %ecx
	jz 3f

	# Update ESP0
	movl 3*4(%ecx), %ecx
	movl %ecx, (tss_begin+4)

	# Update CR3
	movl 0(%eax), %eax		# Get new CR3
	movl %cr3, %edx			# Get current CR3
	cmp %eax, %edx			# Are we switching to the same context?
	jnz 2f
	movl %eax, %cr3			# We aren't, so alter CR3
2:
	movl %ecx, %esp
3:
	popl %ebx
	popl %esi
	popl %edi
	popl %ebp
	xorl %eax, %eax
	ret

/**
 * Entry point of a new thread.
 * Initializes the thread context and does an apropriate iret.
 *
 * Parameters:
 * EBP: pointer to thread block
 * Current stack: Properly formatted iret structure
 */
.extern initialize_thread
.global __initialize_thread
__initialize_thread:
	push %ebp
	call initialize_thread
	addl $4, %esp

	# No need to adjust anything, as we will enter usermode using the method below
	xor %eax, %eax
	xor %ebx, %ebx
	xor %ecx, %ecx
	xor %edx, %edx
	xor %esi, %esi
	xor %edi, %edi
	xor %ebp, %ebp
	iret

/**
 * Enters Ring 3.
 * 
 * Parameters:
 * 4(ESP) / %EDX: Thread register block
 * 8(ESP) / %ECX: Actual usermode entry point
 */
.global enter_usermode
enter_usermode:
	movl 4(%esp), %esi		# Put register block pointer in esi
	movl 8(%esp), %edi

	# Update RSP0
	movl %esp, %edx
	addl $4, %edx			# Exclude return address
	movl %edx, 0(%esi)		# Update KESP
	push %edx
	call set_esp0
	addl $4, %esp

	# Construct IRET Stack
	push $0x23 				# SS
	push 4(%esi) 			# CPL3_RSP

	pushf 					# EFLAGS
	push $0x1B 				# CS
	push %edi 				# EIP

	# Update segment selectors
	movw $0x23, %dx
	movw %dx, %ds
	movw %dx, %es
	movw %dx, %fs
	movw %dx, %gs

	# Clear registers
	xor %eax, %eax
	xor %ebx, %ebx
	xor %ecx, %ecx
	xor %edx, %edx
	xor %esi, %esi
	xor %edi, %edi
	xor %ebp, %ebp
	iret

.extern tss_begin
.global set_esp0
/**
 * Sets ESP0 in the TSS.
 * Clobbers EDX
 * 
 * Parameters:
 * 4(ESP): new RSP value
 */
set_esp0:
	movl 4(%esp), %edx 
	movl %edx, (tss_begin+4)
	ret

.global usermode_code
usermode_code:

	# Print out test string
	xor %eax, %eax
	movl $1, %eax
	movl $(strval - usermode_code + 0x400000), %ebx
	int $0x80

	# Sleep for 1 second
	xor %eax, %eax
	movl $2, %eax
	movl $1000, %ebx
	int $0x80

	# Yield
	xor %eax, %eax
	int $0x80
	jmp usermode_code

strval:
	.asciz "Hello World! "
endcode:
	.skip 4096 - (endcode - usermode_code)