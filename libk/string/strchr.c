#include <string.h>

const char* strchr(const char* str, int chr)
{
    size_t len = strlen(str);
    const char* index = str;

    // Return the end of the string on a null byte
    if(!chr)
        return index + len;

    // Search for char
    while(len--)
    {
        if(*index == (char)chr)
            return index;
        index++;
    }

    return NULL;
}
