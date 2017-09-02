#include <__commons.h>

#if __STDC_HOSTED__ == 1
#error Hosted libc not available
#endif

#ifndef STDLIB_H
#define STDLIB_H

#ifdef __cplusplus
extern "C"
{
#endif

#define EXIT_SUCCESS 0
#define EXIT_FAILURE -1

void abort(void) __attribute__((noreturn));
void exit(int status);
extern char* itoa (int value, char * str, int base);
extern char* ultoa (unsigned long value, char * str, int base);

#ifdef __cplusplus
}
#endif

#endif
