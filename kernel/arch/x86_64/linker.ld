ENTRY(_start)
TARGET(elf32-x86-64)

SECTIONS {

	. = 0xC0100000;
	kernel_start = .;

	.text ALIGN(4K) : AT(ADDR(.text) - 0xC0000000) {
		*(.multiboot)
		*(.text)
	}

	.rodata ALIGN(4K) : AT(ADDR(.rodata) - 0xC0000000) {
		*(.rodata)
	}

	rodata_end = .;

	.data ALIGN(4K) : AT(ADDR(.data) - 0xC0000000) {
		*(.data)
	}

	.bss ALIGN(4K) : AT(ADDR(.bss) - 0xC0000000) {
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
}
