#include <string.h>

int strcmp(const char* str1, const char* str2)
{
    char ca, cb;
    do {
        ca = *(str1++);
        cb = *(str2++);
    } while(ca == cb && ca != '\0' && cb != '\0');
    
    return ca - cb;
}
