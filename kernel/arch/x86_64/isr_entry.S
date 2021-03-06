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
.extern isr_common
.extern syscall_common

.macro isr_err_entry int_num
.globl isr\int_num\()_entry
isr\int_num\()_entry:

    push $\int_num\()
    jmp isr_entry
.endm

.macro isr_noerr_entry int_num
.globl isr\int_num\()_entry
isr\int_num\()_entry:
    push $0
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
irq_entry  0
irq_entry  1
irq_entry  2
irq_entry  3
irq_entry  4
irq_entry  5
irq_entry  6
irq_entry  7
irq_entry  8
irq_entry  9
irq_entry 10
irq_entry 11
irq_entry 12
irq_entry 13
irq_entry 14
irq_entry 15
irq_entry 16
irq_entry 17
irq_entry 18
irq_entry 19
irq_entry 20
irq_entry 21
irq_entry 22
irq_entry 23

.globl spurious_irq
spurious_irq:
    iretq

.extern set_rsp0
.globl syscall_entry
syscall_entry:
    push $0
    push $0x80

    # Push other regs
    push %r12
    push %r13
    push %r14
    push %r15

    push %r8
    push %r9
    push %r10
    push %r11

    push %rbp
    push %rdi
    push %rsi
    push %rdx
    push %rcx
    push %rbx
    push %rax

    movq %rsp, %rdi
    call syscall_common
    
    movq %rsp, %rdi
    addq $(22*8), %rdi
    call set_rsp0

    # Restore registers
    popq %rax
    popq %rbx
    popq %rcx
    popq %rdx
    popq %rsi
    popq %rdi
    popq %rbp //7
    
    popq %r11
    popq %r10
    popq %r9
    popq %r8 //11

    popq %r15
    popq %r14
    popq %r13
    popq %r12 //15

    addq $16, %rsp //17

    iretq // 22

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
    
    movq %rsp, %rdi
    addq $(23*8), %rdi
    call set_rsp0

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

    iretq
