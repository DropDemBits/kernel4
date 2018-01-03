#include <stdio.h>
#include <kfuncs.h>
#include <hal.h>
#include <tty.h>

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
