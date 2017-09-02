#include <stdarg.h>
#include <stdio.h>

int sprintf(char *dest, const char *format, ...)
{
    //TODO: Finish sprintf
    va_list params;
    va_start(params, format);

    int written = vsprintf(dest, format, params);

    va_end(params);
    return written;
}
