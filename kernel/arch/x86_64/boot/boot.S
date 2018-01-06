# Multiboot things
.set MULTIBOOT1_LOADER_MAGIC, 0x2BADB002
.set MULTIBOOT2_LOADER_MAGIC, 0x36D76289

.set KERNEL_BASE, 0xFFFF800000000000 - 0x200000

.section .bss
	.align 4096

stack_top:
	.skip 16384 # 16KiB
stack_bottom:
.global stack_bottom
isr_stack_top:
	.skip 8192 # 8KiB
isr_stack_bottom:
.global isr_stack_bottom

.section .data
	.align 4096
	.global bootstrap_pml4
bootstrap_pml4:
	.quad bootstrap_pdpt0 + 0x003
	.skip 2048 - 8
	.quad bootstrap_pdpt1 - KERNEL_BASE + 0x003
	.skip 2048 - 16
	.quad bootstrap_pml4 - KERNEL_BASE + 0x003
bootstrap_pdpt1:
	.quad bootstrap_pd1 - KERNEL_BASE + 0x003
	.skip 4096 - 8
bootstrap_pd1:
	.quad 0x0000000000200000 + 0x083
	.quad 0x0000000000000000 + 0x083
	.skip 4096 - 16

gdt_begin:
	.long 0, 0
	# R0 Code64 Descriptor
	.word 0xFFFF, 0x0000 # Segment Limit, Base Address (IGNORED LM)
	.byte 0x00 # Base Address (IGNORED LM)
	.byte 0b10011010
	.byte 0b10101111
	.byte 0x00
	# R0 Data64 Descriptor
	.word 0xFFFF, 0x0000
	.byte 0x00
	.byte 0b10010010
	.byte 0b11001111
	.byte 0x00
	# R3 Code64 Descriptor
	.word 0xFFFF, 0x0000
	.byte 0x00
	.byte 0b11111110
	.byte 0b10101111
	.byte 0x00
	# R3 Data64 Descriptor
	.word 0xFFFF, 0x0000
	.byte 0x00
	.byte 0b11110010
	.byte 0b11001111
	.byte 0x00
tss_descriptor:
	# TSS64 Descriptor Low
	.word 0x67
	.word 0 # Base to set
	.byte 0 # Base to set
	.byte 0b10001001
	.byte 0b10000000
	.byte 0 # Base to set
	# TSS64 Descriptor High
	.long 0 # Base to set
	.long 0

gdt_end:
	.align 4096
idt_table:
.global idt_table
	.skip (16 * 256)
idt_end:

gdtr:
	.word (gdt_end - gdt_begin)
	.quad gdt_begin
idtr:
	.word (idt_end - idt_table)
	.quad idt_table

.global tss_begin
tss_begin:
	.long 0
	.quad stack_bottom       # RSP0
	.quad 0xdead000000000000 # RSP1
	.quad 0xdead000000000000 # RSP2
	.quad 0
	.quad isr_stack_bottom   # IST1
	.quad 0xdead000000000000 # IST2
	.quad 0xdead000000000000 # IST3
	.quad 0xdead000000000000 # IST4
	.quad 0xdead000000000000 # IST5
	.quad 0xdead000000000000 # IST6
	.quad 0xdead000000000000 # IST7
	.quad 0
	.word 0
	.word 0                  # IOMAP Base Offset
tss_end:

.section .init.data, "awx"
	# Bootstrap page structures
bootstrap_pdpt0:
	.quad bootstrap_pd0 + 0x003
	.skip 4096 - 8
bootstrap_pd0:
	.quad 0x0000000000000000 + 0x083
	.quad 0x0000000000200000 + 0x083
	.skip 4096 - 16
	# Bootstrap GDT
gdt32_begin:
	.long 0, 0
	# R0 Code32 Descriptor
	.word 0xFFFF, 0x0000 # Segment Limit, Base Address (IGNORED LM)
	.byte 0x00 # Base Address (IGNORED LM)
	.byte 0b10011010
	.byte 0b10101111
	.byte 0x00
	# R0 Data32 Descriptor
	.word 0xFFFF, 0x0000
	.byte 0x00
	.byte 0b10010010
	.byte 0b11001111
	.byte 0x00
gdt32_end:

gdtr32:
	.global gdtr32
	.word (gdt_end - gdt_begin)
	.quad gdt32_begin

.section .text
.global lm_entry, halt
.type lm_entry, @function

.extern kmain, setup_idt, stack_bottom
.extern multiboot_magic, multiboot_ptr

lm_entry:
	# Get rid of bootstap paging structures (Not yet because multiboot stuff)
	#movabsq $bootstrap_pml4, %rdx
	#movq $0x0, (%rdx)

	# Reload CR3
	movq %cr3, %rdx
	movq %rdx, %cr3

	# Fixup TSS Descriptor
	movabsq $tss_begin, %rdx
	movabsq $tss_descriptor, %rdi
	movw %dx, 2(%rdi)
	shrq $16, %rdx
	movb %dl, 4(%rdi)
	movb %dh, 7(%rdi)
	shrq $16, %rdx
	movl %edx, 8(%rdi)

	# Load stack
	movabs $stack_bottom, %rsp

	# Set multiboot things
	movabs $multiboot_magic, %rdx
	movl %eax, (%rdx)
	movabs $multiboot_ptr, %rdx
	movl %ebx, (%rdx)

	# Reload GDTR
	movabsq $gdtr, %rdx
	lgdt (%rdx)

	# Load IDT
	movabsq $idtr, %rdx
	lidt (%rdx)
	call setup_idt

	# Load TSS
	movl $0x28, %edx
	ltr %dx

	jmp kmain
halt:
	cli
	hlt
	jmp halt
