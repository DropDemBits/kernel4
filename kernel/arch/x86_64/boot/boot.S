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

.section .data
	.align 4096
	.global bootstrap_pml4
bootstrap_pml4:
	.quad bootstrap_pdpt0 - KERNEL_BASE + 0x003
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

graphics_pdpt0:
	.quad graphics_pd0 - KERNEL_BASE + 0x003
	.skip 4096 - 8
graphics_pd0:
	.quad graphics_pt0 - KERNEL_BASE + 0x003
	.skip 4096 - 8
graphics_pt0:
	.skip 4096

gdt_begin:
	.long 0, 0
	# R0 Code32 Descriptor
	.word 0xFFFF, 0x0000 # Segment Limit, Base Address (IGNORED LM)
	.byte 0x00 # Base Address (IGNORED LM)
	.byte 0b10011010
	.byte 0b11101111
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
	.byte 0b11101111
	.byte 0x00
	# R3 Data32 Descriptor
	.word 0xFFFF, 0x0000
	.byte 0x00
	.byte 0b11110010
	.byte 0b11001111
	.byte 0x00
	# TSS32 Descriptor

gdt_end:
	.align 4096
idt_begin:
	.skip (8*256)
idt_end:

gdtr:
	.word (gdt_end - gdt_begin)
	.quad gdt_begin
idtr:
	.word (idt_end - idt_begin)
	.quad idt_begin

.section .init.data
	# Bootstrap page structures
bootstrap_pdpt0:
	.quad bootstrap_pd0 - KERNEL_BASE + 0x003
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
	.quad gdt32_begin - KERNEL_BASE

.section .text
.global lm_entry, halt
.type lm_entry, @function

.extern kmain, stack_bottom
.extern multiboot_magic, multiboot_ptr

lm_entry:
	# Get rid of bootstap paging structures (Not yet because multiboot stuff)
	#movabsq $bootstrap_pml4, %rdx
	#movq $0x0, (%rdx)

	# Add Graphics structures (temporary)
	movabs $bootstrap_pml4, %rdx
	movq $(graphics_pdpt0 - KERNEL_BASE + 0x003), 448*8(%rdx)

	# Reload CR3
	movq %cr3, %rdx
	movq %rdx, %cr3

	# Load IDT
	movabsq $idtr, %rdx
	lidt (%rdx)

	# Reload GDTR
	movabsq $gdtr, %rdx
	lgdt (%rdx)

	# Load stack
	movabs $stack_bottom, %rsp

	# Set multiboot things
	movabs $multiboot_magic, %rdx
	movl %eax, (%rdx)
	movabs $multiboot_ptr, %rdx
	movl %ebx, (%rdx)

	jmp kmain
halt:
	cli
	hlt
	jmp halt
