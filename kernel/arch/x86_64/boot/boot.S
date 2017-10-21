
.section .bss
	.align 4096

stack_top:
	.skip 16384 # 16KiB
stack_bottom:

mb2_ptr:
	.quad 0
mb1_ptr:
	.quad 0

.section .data
	.align 4096
	.global bootstrap_pml4
bootstrap_pml4:
	.quad bootstrap_pdpt + 0x003
	.skip 4096 - 8
bootstrap_pdpt:
	.quad bootstrap_pd + 0x003
	.skip 4096 - 8
bootstrap_pd:
	.quad 0x0000000000000000 + 0x083
	.quad 0x0000000000200000 + 0x083
	.skip 4096 - 16
	# GDT32
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

.section .text
.global lm_entry, halt
.type lm_entry, @function

.extern kmain, stack_bottom

lm_entry:
	# Load IDT
	movabsq $idtr, %rdx
	lidt (%rdx)

	# Reload GDTR
	movabsq $gdtr, %rdx
	lgdt (%rdx)

	movabs $stack_bottom, %rsp

	#jmp kmain
halt:
	cli
	hlt
	jmp halt
