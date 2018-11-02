#include <stdarg.h>
#include <stdio.h>

int snprintf(char *dest, size_t num, const char *format, ...)
{
    va_list params;
    va_start(params, format);

    int written = vsnprintf(dest, num, format, params);

    va_end(params);
    return written;
}
