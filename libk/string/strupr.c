#include <string.h>
#include <ctype.h>

char* _strupr(char *s)
{
    char* start = s;

    while(*s)
        *(s++) = toupper(*s);

    return start;
}