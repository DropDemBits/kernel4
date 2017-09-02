#include <string.h>

void* memset(void* str, const int c, size_t num)
{
    //TODO: implement faster memset
    unsigned char* strdest = (unsigned char*) str;
    for(size_t i = 0; i < num; i++) strdest[i] = c;
    return strdest;
}
