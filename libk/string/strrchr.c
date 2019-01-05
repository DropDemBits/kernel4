#include <string.h>

const char* strrchr(const char* str, int chr)
{
    size_t len = strlen(str);
    const char* index = str + len;

    if(!chr)
        return index;

    // Skip null byte
    len--;
    index--;

    // Search for char
    while(len--)
    {
        if(*index == (char)chr)
            return index;
        index--;
    }

    return NULL;
}
