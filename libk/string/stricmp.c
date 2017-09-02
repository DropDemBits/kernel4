#include <string.h>
#include <ctype.h>

#ifdef __cplusplus__strings__
int stricmp(const char* str1, const char* str2)
{
    char ca, cb;
    do {
        ca = *(str1++);
        cb = *(str2++);
        ca = toLower(toUpper(ca));
        cb = toLower(toUpper(cb));
    } while(ca == cb && ca != '\0' && cb != '\0');
    
    return ca - cb;
}
#endif
