#include <string.h>

void* memcpy(void* dest, const void* src, size_t num)
{
    //TODO: implement faster copy, and align
    if(dest == src) return dest;
    
    unsigned char *cdest = (unsigned char*) dest;
    unsigned char *csrc = (unsigned char*) src;
    for(size_t i = 0; i < num; i++)
        cdest[i] = csrc[i];
    return dest;
}

