ENTRY(_start)
TARGET(elf32-i386)
KERNEL_VMA = 0x80100000;

SECTIONS {

	. = KERNEL_VMA;
	kernel_start = .;
	kernel_phystart = . - KERNEL_VMA + 0x100000;
	.init ALIGN(4K) : AT(ADDR(.init) - 0x80000000) {
		*(.multiboot)
		*(init.code)
		*(init.data)
		*(init.bss)
	}

	.text ALIGN(4K) : AT(ADDR(.text) - 0x80000000) {
		*(.text)
	}

	.rodata ALIGN(4K) : AT(ADDR(.rodata) - 0x80000000) {
		*(.rodata)
	}

	rodata_end = .;

	.data ALIGN(4K) : AT(ADDR(.data) - 0x80000000) {
		*(.data)
	}

	.bss ALIGN(4K) : AT(ADDR(.bss) - 0x80000000) {
		*(.COMMON)
		*(.bss)
		*(.bootstrap_stack)
	}

	kernel_end = .;

	/DISCARD/ : {
		*(.comment)
		*(.eh_frame)
	}

	kernel_end = .;
	kernel_page_end = ALIGN(4K);
	kernel_physize = kernel_page_end - kernel_start;
}
