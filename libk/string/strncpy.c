#include <string.h>

void* strncpy(char* dest, const char* src, size_t num)
{
    if(dest == src) return dest;
    
    if(dest < src)
        for(size_t i = 0; i < num || !src[i]; i++)
            dest[i] = src[i];
    else
        for(size_t i = num; i != 0 || !src[i]; i--)
            dest[i-1] = src[i-1];
    return dest;
}

