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

__EXPORT_SPEC__ void abort(void) __attribute__((noreturn));
__EXPORT_SPEC__ void exit(int status);
__EXPORT_SPEC__ char* itoa (int value, char * str, int base);
__EXPORT_SPEC__ char* lltoa (long long value, char * str, int base);
__EXPORT_SPEC__ char* ulltoa (unsigned long long value, char * str, int base);
__EXPORT_SPEC__ unsigned long int strtoul (const char* str, char** endptr, int base);

__EXPORT_SPEC__ int atoi (const char * str);
__EXPORT_SPEC__ long int atol (const char * str);

#ifdef __cplusplus
}
#endif

#endif
