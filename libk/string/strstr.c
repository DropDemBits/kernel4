#include <string.h>

// Find str2 inside of str1
const char* strstr (const char* str1, const char* str2)
{
    size_t substr_len = strlen(str2);
    size_t compare_len = strlen(str1);

    // If the substring is bigger than the main string, it's most likely not going to be there
    if(substr_len > compare_len)
        return NULL;

    compare_len -= substr_len;

    char* start = str1;

    while(strncmp(str2, start, substr_len) != 0 && compare_len--)
        start++;

    if(compare_len == 0)
        return NULL;
    return start;
}
