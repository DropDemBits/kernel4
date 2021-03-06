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
 * RDI: Pointer to new thread
 * RSI: Pointer to old thread
 * RDX: Pointer to new thread's paging context
 */
.global switch_stack
switch_stack:
    push %rbp
    push %rbx
    push %r12
    push %r13
    push %r14
    push %r15

    movq %rdx, %r11

    # Update KRSP in old thread
    movq $0xFFFFDEAD00000000, %rax
    cmpq %rax, %rsi
    jz 1f
    movq %rsp, 2*8(%rsi)
1:
    # Skip stack change (if needed)
    cmpq %rax, %rdi
    jz 3f

    # Update RSP0
    movabsq $tss_begin, %rdx
    movq 2*8(%rdi), %rdi    # KESP
    movq %rdi, 4(%rdx)        # RSP0

    # Change CR3 (if needed)
    movq 0(%r11), %r11        # Get New CR3
    movq %cr3, %rdx
    cmpq %rdx, %r11            # Going back to old CR3?
    jz 2f

    movq %r11, %cr3            # Change CR3 to new context
2:
    # Change stack
    movq %rdi, %rsp
3:
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
.extern initialize_thread
.global __initialize_thread
__initialize_thread:
    movq %rbp, %rdi
    call initialize_thread

    # Cleanup registers
    xor %rbp, %rbp
    xor %r11, %r11
    xor %r10, %r10
    xor %r9, %r9
    xor %r8, %r8
    xor %rsi, %rsi
    xor %rax, %rax
    xor %rdx, %rdx
    xor %rcx, %rcx

    # Move parmeters into first register
    movq %rbx, %rdi
    xor %rbx, %rbx

    iretq

/**
 * Enters Ring 3.
 * 
 * Parameters:
 * RDI: Thread control block
 * RSI: Actual usermode entry point
 */
.global enter_usermode
enter_usermode:
    movq 2*8(%rdi), %rdi    # TCB->KRSP
    movq %rdi, %r11            # Store KRSP

    # Update RSP0
    movq %rsp, %rdi
    addq $8, %rdi            # Exclude return address
    movq %rdi, 0(%r11)        # Update KRSP
    call set_rsp0

    # Construct IRET Stack
    # 0x1B = R3 Code, 0x23 = R3 Data

    push $0x23 # SS
    push 8(%r11) # CPL3_RSP

    pushf # RFLAGS
    push $0x1B # CS
    push %rsi # RIP

    # Cleanup Registers
    xor %r15, %r15
    xor %r14, %r14
    xor %r13, %r13
    xor %r12, %r12
    xor %r11, %r11
    xor %r10, %r10
    xor %r9, %r9
    xor %r8, %r8
    xor %rbp, %rbp
    xor %rdi, %rdi
    xor %rsi, %rsi
    xor %rdx, %rdx
    xor %rcx, %rcx
    xor %rbx, %rbx
    xor %rax, %rax
    iretq

.extern tss_begin
.global set_rsp0
/**
 * Sets RSP0 in the TSS.
 * Trashes rdx
 *
 * Parameters:
 * RDI: new RSP value
 */
set_rsp0:
    movabsq $tss_begin, %rdx
    movq %rdi, 4(%rdx)
    ret
