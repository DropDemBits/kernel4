#include <string.h>

int strncmp(const char* str1, const char* str2, size_t num)
{
    if(str1 == str2) return 0;
    
    char ca, cb;
    do {
        ca = *(str1++);
        cb = *(str2++);
    } while(ca == cb && num-- > 1 && ca != '\0' && cb != '\0');
    
    return ca - cb;
}
