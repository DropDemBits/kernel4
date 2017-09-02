#include <stdarg.h>
#include <stdio.h>

int printf(const char *format, ...)
{
    va_list params;
    va_start(params, format);

    int written = vprintf(format, params);

    va_end(params);
    return written;
}
