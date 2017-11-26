ENTRY(_start)
BOOTSTRAP_VMA = 1M;
KERNEL_VMA = 0xFFFF800000000000;
OUTPUT_FORMAT(elf64-x86-64)

SECTIONS {

	. = BOOTSTRAP_VMA;
	.bootstrap : ALIGN(4K) {
		*(.multiboot)
		*bootstrap.o (.text, .text.*)
		*bootstrap.o (.rodata, .rodata.*)
		*bootstrap.o (.data, .data.*)
		*bootstrap.o (.bss)
	}

	.init : ALIGN(4K) {
		*(.init.code)
		*(.init.data)
		*(.init.bss)
		*(.init.*)
	}

	. = KERNEL_VMA;
	kernel_start = .;
	kernel_phystart = . - KERNEL_VMA + 0x200000;

	.text ALIGN(4K) : AT(ADDR(.text) - KERNEL_VMA + 0x200000) {
		*(.text)
	}

	.rodata ALIGN(4K) : AT(ADDR(.rodata) - KERNEL_VMA + 0x200000) {
		*(.rodata)
	}

	rodata_end = .;

	.data ALIGN(4K) : AT(ADDR(.data) - KERNEL_VMA + 0x200000) {
		*(.data)
	}

	.bss ALIGN(4K) : AT(ADDR(.bss) - KERNEL_VMA + 0x200000) {
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
