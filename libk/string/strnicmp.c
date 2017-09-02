#include <string.h>
#include <ctype.h>

int strnicmp(const char* str1, const char* str2, size_t num)
{
    char ca, cb;
    do {
        ca = *(str1++);
        cb = *(str2++);
        ca = tolower(toupper(ca));
        cb = tolower(toupper(cb));
    } while(ca == cb && num-- > 1 && ca != '\0' && cb != '\0');
    
    return ca - cb;
}
