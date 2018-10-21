#include <string.h>

char* strcat (char* dest, const char* src)
{
    size_t src_len = strlen(src);
    size_t dest_len = strlen(dest);

    // Overlapping, return immediately
    if(dest >= src && (dest + dest_len) <= (src + src_len))
        return dest;

    // Directly append onto the end
    char* dest_chars = dest + dest_len;
    char* src_chars = src;
    while(*src_chars)
        *(dest_chars++) = *(src_chars++);

    // Append null terminator
    *(dest_chars) = '\0';

    return dest;
}
