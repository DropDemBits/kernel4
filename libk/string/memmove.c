#include <string.h>
#include <math.h>

void* memmove(void* dest, const void* src, size_t num)
{
    if(dest == src) return 0;
    
    //TODO: implement faster moving (arch specific?)
    unsigned char *cdest = (unsigned char*) dest;
    unsigned char *csrc = (unsigned char*) src;
    if(cdest < csrc)
        for(size_t i = 0; i < num; i++)
            cdest[i] = csrc[i];
    else
        for(size_t i = num; i != 0; i--)
            cdest[i-1] = csrc[i-1];
    return dest;
}

