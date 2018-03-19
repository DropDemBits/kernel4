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

.global switch_stack
.global initialize_thread
.extern tss_begin

/**
 * Switches the stack to the new pointer and saves the old into the thread's
 * register state. Old rsp will not be saved if rdi is KNULL, and the stack will
 * not change if rsi is KNULL.
 *
 * Parameters:
 * 4(ESP): Pointer to new thread's register state (ECX)
 * 8(ESP): Pointer to old thread's register state (EDX)
 */
switch_stack:
	push %ebp
	push %edi
	push %esi
	push %ebx

	movl 20(%esp), %ecx
	movl 24(%esp), %edx

	cmpl $0xDEADBEEF, %edx
	jz 1f
	movl %esp, 0(%edx)
1:
	testl $0xDEADBEEF, %ecx
	jz 2f
	movl 0(%ecx), %ecx
	movl %ecx, (tss_begin+4)
	movl %ecx, %esp
2:
	popl %ebx
	popl %esi
	popl %edi
	popl %ebp
	sti
	ret

/**
 * Entry point of a new thread.
 * Initializes the thread context and does an apropriate iret.
 *
 * Parameters:
 * EBP: pointer to thread block
 * Current stack: Properly formatted iret structure
 */

initialize_thread:
	# Set thread state to running
	movl $1, 12(%ebp)
	xor %ebp, %ebp

	/*
	 * 16 SS
	 * 12 ESP
	 *  8 EFLAGS
	 *  4 CS
	 *  0 EIP
	 */

	cmpb $0x08, 4(%esp)
	jnz 1f

	movl 12(%esp), %eax
	movl 8(%esp), %ecx
	movl 0(%esp), %edx

	subl $12, %eax
	movl %edx, 0(%eax)
	movl $0x8, 4(%eax)
	movl %ecx, 8(%eax)

	movl %eax, %esp

1:
	xor %eax, %eax
	xor %ecx, %ecx
	xor %edx, %edx
	iret
