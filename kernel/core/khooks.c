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
}

void kexit(int status)
{
	kpanic("Exit called with code %d", status);
}
