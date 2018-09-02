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

.extern isr_common, set_esp0

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

.global syscall_entry
.extern set_esp0

syscall_entry:
    push $0x80
    push $0x00

    push %ds
    push %es
    push %fs
    push %gs

    # Push other regs
    push %ebp
    push %edi
    push %esi
    push %edx
    push %ecx
    push %ebx
    push %eax

    movw $0x10, %dx
    movw %dx, %ds
    movw %dx, %es
    movw %dx, %fs
    movw %dx, %gs

    # Call syscall_common
    movl %esp, %eax
    push %eax
    call syscall_common
    addl $4, %esp

    # Since this will always be entered from usermode, we don't have to worry
    # about adjusting the stack offset
    movl %esp, %edx
    addl $(18*4), %edx
    push %edx
    call set_esp0
    addl $4, %esp

    # Restore registers
    popl %eax
    popl %ebx
    popl %ecx
    popl %edx
    popl %esi
    popl %edi
    popl %ebp // 7

    popl %gs
    popl %fs
    popl %es
    popl %ds // 11

    addl $8, %esp // 13

    iret // 18

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

    # Update segment selectors
    push %ds
    push %es
    push %fs
    push %gs

    movw $0x10, %dx
    movw %dx, %ds
    movw %dx, %es
    movw %dx, %fs
    movw %dx, %gs

    # Call isr_common
    movl %esp, %eax
    push %eax
    call isr_common
    addl $4, %esp

    # Since we really only need to adjust ESP0 when returning to usermode,
    # we can skip it when returning to kernel mode
    movl 11*4(%esp), %ecx        # Fetch returning CPL
    cmp $0x08, %ecx                # Are we returning to Kernel mode?
    jz 1f

    # Change ESP0
    movl %esp, %edx
    addl $(19*4), %edx        # Regs, IntCode, ErrCode, Usermode IRET
    push %edx
    call set_esp0
    addl $4, %esp
1:
    # Restore registers
    popl %gs
    popl %fs
    popl %es
    popl %ds

    popl %eax
    popl %edx
    popl %ecx

    popl %ebx
    movl %ebx, %cr2

    popl %ebx
    popl %edi
    popl %esi
    popl %ebp

    addl $8, %esp // 10

    iret // 15 (User mode), 13 (Kernel mode)
