#include <kernhooks.h>
#include <kfuncs.h>
#include <tty.h>

void kabort()
{
	kpanic("Abort called");
}

void kputchar(int ic)
{
	tty_printchar(ic);
	if(ic == '\n') tty_reshow();
}

void kexit(int status)
{
	kpanic("Exit called with code %d", status);
}
