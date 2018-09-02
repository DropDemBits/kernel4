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
