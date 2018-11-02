#include <__commons.h>
#include <stdarg.h>

#ifndef STDIO_H
#define STDIO_H

#ifdef __cplusplus
extern "C"
{
#endif

// Macros
#define EOF -1

__EXPORT_SPEC__ int printf(const char *format, ...);
__EXPORT_SPEC__ int sprintf(char *dest, const char *format, ...);
__EXPORT_SPEC__ int vprintf(const char *format, va_list params);
__EXPORT_SPEC__ int vsprintf(char *dest, const char *format, va_list params);
__EXPORT_SPEC__ int snprintf(char *dest, size_t n, const char *format, ...);
__EXPORT_SPEC__ int vsnprintf(char *dest, size_t n, const char *format, va_list params);
__EXPORT_SPEC__ int puts(const char *str);
__EXPORT_SPEC__ int putchar(int ic);

#ifdef __cplusplus
}
#endif

#endif
