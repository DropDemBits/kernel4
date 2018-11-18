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

# Multiboot things
#define MULTIBOOT_MAGIC     0x1BADB002
#define ALIGN_MODULES       0b00000001
#define REQUEST_MMAP        0b00000010
#define FRAMEBUFFER         0b00000100
#define MULTIBOOT_FLAGS     (ALIGN_MODULES | REQUEST_MMAP | FRAMEBUFFER)
#define MULTIBOOT_CHECKSUM  -(MULTIBOOT_MAGIC + MULTIBOOT_FLAGS)

# Multiboot2 things
#define MULTIBOOT2_MAGIC    0xE85250D6
#define MULTIBOOT2_ARCH     0
#define MULTIBOOT2_LENGTH   (multiboot2_header_end - multiboot2_header_start)
#define MULTIBOOT2_CHECKSUM -(MULTIBOOT2_MAGIC + MULTIBOOT2_ARCH + \
    MULTIBOOT2_LENGTH)

# Framebuffer preferences
#define FRAMEBUFFER_TYPE    0
#define FRAMEBUFFER_WIDTH   640
#define FRAMEBUFFER_HEIGHT  480
#define FRAMEBUFFER_BPP     16

.set KERNEL_VIRTUAL_BASE, 0x80000000

.section .multiboot, "awx"
    # Multiboot2 Header
    .align 8
multiboot2_header_start:
    .long MULTIBOOT2_MAGIC
    .long MULTIBOOT2_ARCH
    .long MULTIBOOT2_LENGTH
    .long MULTIBOOT2_CHECKSUM

    # Multiboot2 Tags

    # Request Tags
    tag1_start:
    .word 1
    .word 0x0000
    .long tag1_end - tag1_start
    .long 0x00000004 # Basic Meminfo
    .long 0x00000006 # MMap
    .long 0x00000008 # Framebuffer
    tag1_end:

    .long 0 # Same as .align 8
    tag5_start:
    .word 5 # Frambuffer
    .word 0x0001
    .long tag5_end - tag5_start
    .long FRAMEBUFFER_WIDTH
    .long FRAMEBUFFER_HEIGHT
    .long FRAMEBUFFER_BPP
    tag5_end:

    .long 0 # Same as .align 8
    tag6_start:
    .word 6 # Align modules to page boundaries
    .word 0x0000
    .long tag6_end - tag6_start
    tag6_end:

    .align 8
    # End Of Header
    .long 0, 8
multiboot2_header_end:
    .align 4
    # Multiboot Header
    .long MULTIBOOT_MAGIC
    .long MULTIBOOT_FLAGS
    .long MULTIBOOT_CHECKSUM

    # Addresses (Unused)
    .long 0
    .long 0
    .long 0
    .long 0
    .long 0

    # Framebuffer Preferences
    .long FRAMEBUFFER_TYPE
    .long FRAMEBUFFER_WIDTH
    .long FRAMEBUFFER_HEIGHT
    .long FRAMEBUFFER_BPP

.section .bss
    .align 4096
bootstack_bottom:
.global bootstack_bottom
    .skip 16384 # 16KiB
bootstack_top:
.global bootstack_top

bootstrap_pd:
    .skip 4096
bootstrap_pt0:
    .skip 4096
bootstrap_pt1:
    .skip 4096

.section .data
    .align 4096
    # GDT32
gdt_begin:
    .long 0, 0
    # R0 Code32 Descriptor
    .word 0xFFFF, 0x0000
    .byte 0x00
    .byte 0b10011010
    .byte 0b11001111
    .byte 0x00
    # R0 Data32 Descriptor
    .word 0xFFFF, 0x0000
    .byte 0x00
    .byte 0b10010010
    .byte 0b11001111
    .byte 0x00
    # R3 Code32 Descriptor
    .word 0xFFFF, 0x0000
    .byte 0x00
    .byte 0b11111110
    .byte 0b11001111
    .byte 0x00
    # R3 Data32 Descriptor
    .word 0xFFFF, 0x0000
    .byte 0x00
    .byte 0b11110010
    .byte 0b11001111
    .byte 0x00
tss_descriptor:
    # TSS32 Descriptor
    .word 0x67
    .word 0 # Base to set
    .byte 0 # Base to set
    .byte 0b10001001
    .byte 0b10000000
    .byte 0 # Base to set

gdt_end:
    .align 4096
idt_table:
    .skip (8*256)
idt_end:
.global idt_table

gdtr:
    .word (gdt_end - gdt_begin)
    .long gdt_begin
idtr:
    .word (idt_end - idt_table)
    .long idt_table

.global tss_begin
tss_begin:
    .word 0 # Previous task link
    .word 0
    .long bootstack_top # ESP0
    .long 0x10 # SS0
    .skip 4*23
tss_end:

.section .text
.global _start, halt
.type _start, @function

.extern kernel_start, kernel_end, kmain, setup_idt
.extern multiboot_ptr, multiboot_magic
_start:
    cli

    # Setup paging
    movl $0, %esi                                      # Phyiscal Address
    movl $0, %edx                                      # Scratch
    movl $(bootstrap_pt0 - KERNEL_VIRTUAL_BASE), %edi  # PTE
    movl $4096, %ecx                                   # Map 4096 Pages

begin:
    #cmp $(kernel_start - KERNEL_VIRTUAL_BASE), %esi
    #jl skip
    cmp $(kernel_end - KERNEL_VIRTUAL_BASE + 0x8000), %esi
    jg done
    cmp $(rodata_end - KERNEL_VIRTUAL_BASE), %esi
    jle ro

    # Read/Write Page
    movl %esi, %edx
    orl $0x003, %edx
    movl %edx, (%edi)
    jmp skip
ro:
    # Read Only Page
    movl %esi, %edx
    orl $0x001, %edx
    movl %edx, (%edi)
skip:
    addl $0x1000, %esi
    addl $4, %edi
    loop begin
done:
    # Identity (Just map the first 8 MiB, the specialized tables are really only needed for the kernel pages)
    movl $(0x00000000 + 0x083), bootstrap_pd - KERNEL_VIRTUAL_BASE + 0
    movl $(0x00400000 + 0x083), bootstrap_pd - KERNEL_VIRTUAL_BASE + 4

    # HH
    movl $(bootstrap_pt0 - KERNEL_VIRTUAL_BASE + 0x003), bootstrap_pd - KERNEL_VIRTUAL_BASE + (KERNEL_VIRTUAL_BASE >> 20) + 0
    movl $(bootstrap_pt1 - KERNEL_VIRTUAL_BASE + 0x003), bootstrap_pd - KERNEL_VIRTUAL_BASE + (KERNEL_VIRTUAL_BASE >> 20) + 4

    # Recursive mapping
    movl $(bootstrap_pd - KERNEL_VIRTUAL_BASE + 0x003), (bootstrap_pd - KERNEL_VIRTUAL_BASE + 0xffc)

    movl %cr4, %edx
    orl $0x00000090, %edx # PGE, PSE
    movl %edx, %cr4

    movl $(bootstrap_pd - KERNEL_VIRTUAL_BASE), %edx
    movl %edx, %cr3

    movl %cr0, %edx
    orl $0x80000000, %edx # Paging
    movl %edx, %cr0

    leal cont, %edx

    jmp *%edx

cont:
    # Fixup TSS Descriptor
    movl $tss_begin, %edx
    movl $tss_descriptor, %edi
    movw %dx, 2(%edi)
    shrl $16, %edx
    movb %dl, 4(%edi)
    movb %dh, 7(%edi)

    # Load GDT
    lgdt (gdtr)

    # Load TSS
    movl $0x28, %edx
    ltr %dx

    mov $0x10, %dx
    mov %dx, %ds
    mov %dx, %ss
    # Setup stack
    movl $bootstack_top, %esp
    subl $10*4, %esp
    mov %dx, %es
    mov %dx, %fs
    mov %dx, %gs
    ljmp $0x8, $flush_cs
flush_cs:

    # Set multiboot pointer
    movl %eax, (multiboot_magic)
    movl %ebx, (multiboot_ptr)

    # Load IDTR
    lidt (idtr)
    call setup_idt

    jmp kmain

.code32
halt:
    cli
    hlt
    jmp halt
