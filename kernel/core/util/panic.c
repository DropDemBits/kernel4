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

#include <stdio.h>
#include <common/kfuncs.h>
#include <common/hal.h>
#include <common/tty.h>

extern void __attribute__((noreturn)) halt();

void __attribute__((noreturn)) kpanic(const char* message_string, ...)
{
	va_list args;

	putchar('\n');
	printf("Exception occured in kernel: ");
	va_start(args, message_string);
	vprintf(message_string, args);
	va_end(args);
	putchar('\n');

	tty_reshow();
	halt();
}

void __attribute__((noreturn)) kpanic_intr(struct intr_stack *stack, const char* message_string, ...)
{
	va_list args;

	putchar('\n');
	printf("Exception occured in kernel: ");
	va_start(args, message_string);
	vprintf(message_string, args);
	va_end(args);
	putchar('\n');
	dump_registers(stack);

	tty_reshow();
	halt();
}

void __attribute__((noreturn)) kvpanic(const char* message_string, va_list args)
{
	putchar('\n');
	printf("Exception occured in kernel: ");
	vprintf(message_string, args);
	putchar('\n');

	tty_reshow();
	halt();
}

void __attribute__((noreturn)) kvpanic_intr(struct intr_stack *stack, const char* message_string, va_list args)
{
	putchar('\n');
	printf("Exception occured in kernel: ");
	vprintf(message_string, args);
	putchar('\n');
	dump_registers(stack);

	tty_reshow();
	halt();
}
