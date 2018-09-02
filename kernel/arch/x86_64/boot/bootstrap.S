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

.set KERNEL_BASE, 0xFFFF800000000000 - 0x200000
.set BOOTSTRAP_VMA, 0x00000000000000

.section .multiboot
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

	.align 8
	tag5_start:
	.word 5 # Frambuffer
	.word 0x0001
	.long tag5_end - tag5_start
	.long FRAMEBUFFER_WIDTH
	.long FRAMEBUFFER_HEIGHT
	.long FRAMEBUFFER_BPP
	tag5_end:

	.align 8
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

.section .rodata
err_not_lm:
	.asciz "Error: This cpu doesn't support the x86_64 arch."

.section .text
.global _start
.type _start, @function

.extern kernel_start, kernel_end, lm_entry, halt
.extern bootstrap_pml4, gdtr32
.code32
_start:
	cli

	# Check for LM
	movl %eax, %esi
	movl %ebx, %edi

	movl $0x80000001, %eax
	cpuid
	testl $0x20000000, %edx
	jz .not_lm

	# Set LME & NXE in EFER
	movl $0xC0000080, %ecx
	movl $0x00000900, %eax # NXE, LME
	movl $0x00000000, %edx
	wrmsr

	# Preserve the contents of the esi and edi registers
	movl %esi, %ebp
	movl %edi, %ebx

	# Enable Paging
	movl %cr4, %edx
	orl $0x000000A0, %edx # PGE, PAE
	movl %edx, %cr4

	movl $(bootstrap_pml4 - KERNEL_BASE), %edx
	movl %edx, %cr3

	movl %cr0, %edx
	orl $0x80010000, %edx # Paging & WP
	movl %edx, %cr0

	# At this point, we are in Compatibility Mode
	# Load GDT
	lgdt (gdtr32)

	# Flush Everything
	movw $0x10, %dx
	movw %dx, %ds
	movw %dx, %ss
	movw %dx, %es
	movw %dx, %fs
	movw %dx, %gs

	jmp $0x08, $(flush_cs - KERNEL_BASE)

flush_cs:
.code64 # We are now in long mode proper

	# Restore old value of EAX
	movq %rbp, %rax

	# Jump to boot lm_entry
	movabs $lm_entry, %rdx

	jmp *%rdx

.code32
.not_lm:
	jmp halt
