#include <string.h>

void* strcopy(char* dest, const char* src)
{
    if(dest == src) return dest;
    
    size_t num;
    if(strlen(src) > strlen(dest)) num = strlen(src);
    else num = strlen(dest);
    
    if(dest < src)
        for(size_t i = 0; i < num; i++)
            dest[i] = src[i];
    else
        for(size_t i = num; i != 0; i--)
            dest[i-1] = src[i-1];
    return dest;
}

