#include <kernhooks.h>
#include <tty.h>

extern void halt();

void kabort()
{
	halt();
}

void kputchar(int ic)
{
	tty_printchar(ic);
	if(ic == '\n') tty_reshow();
}

void kexit(int status)
{
	halt();
}
