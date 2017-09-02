#include <string.h>

int memcmp(const void* ptr1, const void* ptr2, size_t num)
{
    if(ptr1 == ptr2) return 0;
    
    char* str1 = (char*)ptr1;
    char* str2 = (char*)ptr2;
    
    char ca, cb;
    do {
        ca = *(str1++);
        cb = *(str2++);
    } while(ca == cb && num-- > 1 && ca != '\0' && cb != '\0');
    
    return ca - cb;
}
