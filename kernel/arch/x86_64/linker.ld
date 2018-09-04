/**
 * Copyright (C) 2018 DropDemBits
 * 
 * This file is part of Kernel4.
 * 
 * Kernel4 is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * Kernel4 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with Kernel4.  If not, see <http://www.gnu.org/licenses/>.
 * 
 */

ENTRY(_start)
BOOTSTRAP_VMA = 1M;
KERNEL_VMA = 0xFFFF800000000000;
OUTPUT_FORMAT(elf64-x86-64)

SECTIONS {

    . = BOOTSTRAP_VMA;
    .bootstrap : ALIGN(4K) {
        *(.multiboot)
        *bootstrap.* (.text, .text.*)
        *bootstrap.* (.rodata, .rodata.*)
        *bootstrap.* (.data, .data.*)
        *bootstrap.* (.bss)
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
